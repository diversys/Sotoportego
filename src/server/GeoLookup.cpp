/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "GeoLookup.h"

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <Message.h>
#include <OS.h>
#include <String.h>


namespace GeoLookup {

const char* const kFieldCountry = "soto:geo:country";
const char* const kFieldQueryIP = "soto:geo:queryIP";

// ip-api.com supports a plain-text "line" format keyed by the fields you
// request, so we ask for `country` and `query` and parse the response as
// two newline-separated values in the order we requested them. This avoids
// dragging a JSON parser into the daemon.
static const char* const kHost		= "ip-api.com";
static const int kPort				= 80;
static const char* const kPath		= "/line?fields=country,query";

// Total wall-clock budget for the request -- connect, write, read. If the
// VPN's egress blocks port 80 or ip-api throttles us, we'd rather time out
// quickly and fall back to a country-less notification than make the user
// wait.
static const int kTimeoutSeconds	= 4;

}	// namespace GeoLookup


struct LookupArgs {
	BMessenger	target;
	uint32		what;
};


static void
set_socket_timeout(int fd, int seconds)
{
	struct timeval tv;
	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}


// Synchronous HTTP/1.0 GET. Returns the response body (trimmed) on success,
// or an empty BString on any failure.
static BString
http_get(const char* host, int port, const char* path)
{
	struct hostent* he = gethostbyname(host);
	if (he == NULL || he->h_addr_list == NULL || he->h_addr_list[0] == NULL)
		return BString();

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return BString();

	// Don't leak this short-lived socket into any openvpn child the
	// backend may spawn while we are mid-lookup.
	fcntl(sock, F_SETFD, FD_CLOEXEC);

	set_socket_timeout(sock, GeoLookup::kTimeoutSeconds);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		close(sock);
		return BString();
	}

	BString request;
	request << "GET " << path << " HTTP/1.0\r\n"
		<< "Host: " << host << "\r\n"
		<< "User-Agent: Sotoportego\r\n"
		<< "Accept: text/plain\r\n"
		<< "Connection: close\r\n\r\n";

	const char* req = request.String();
	size_t left = request.Length();
	while (left > 0) {
		ssize_t sent = send(sock, req, left, 0);
		if (sent <= 0) {
			close(sock);
			return BString();
		}
		req += sent;
		left -= sent;
	}

	BString response;
	char buf[1024];
	while (true) {
		ssize_t got = recv(sock, buf, sizeof(buf), 0);
		if (got <= 0)
			break;
		response.Append(buf, got);
	}
	close(sock);

	int32 bodyStart = response.FindFirst("\r\n\r\n");
	if (bodyStart < 0)
		return BString();
	BString body;
	response.CopyInto(body, bodyStart + 4,
		response.Length() - bodyStart - 4);
	body.Trim();

	// Sanity: ip-api can return a one-word country name. Anything that
	// starts with '{' looks like JSON (a fields error from upstream); skip
	// it rather than show garbage in the notification.
	if (body.Length() == 0 || body[0] == '{' || body[0] == '<')
		return BString();
	return body;
}


static int32
lookup_thread(void* arg)
{
	LookupArgs* args = (LookupArgs*)arg;

	BString body = http_get(GeoLookup::kHost, GeoLookup::kPort,
		GeoLookup::kPath);

	// `?fields=country,query` returns two lines in the request order:
	//   Italy
	//   1.2.3.4
	// If the body has only one line we treat it as the country (the older
	// single-field response), so dropping the IP isn't a hard error.
	BString country;
	BString queryIP;
	if (body.Length() > 0) {
		int32 newline = body.FindFirst('\n');
		if (newline < 0) {
			country = body;
		} else {
			body.CopyInto(country, 0, newline);
			body.CopyInto(queryIP, newline + 1, body.Length() - newline - 1);
		}
		country.Trim();
		queryIP.Trim();
	}

	BMessage result(args->what);
	if (country.Length() > 0)
		result.AddString(GeoLookup::kFieldCountry, country);
	if (queryIP.Length() > 0)
		result.AddString(GeoLookup::kFieldQueryIP, queryIP);
	args->target.SendMessage(&result);

	delete args;
	return B_OK;
}


void
GeoLookup::BackgroundLookup(const BMessenger& target, uint32 what)
{
	if (!target.IsValid())
		return;

	LookupArgs* args = new LookupArgs;
	args->target = target;
	args->what = what;

	thread_id thread = spawn_thread(lookup_thread, "geo-lookup",
		B_LOW_PRIORITY, args);
	if (thread < B_OK) {
		delete args;
		return;
	}
	resume_thread(thread);
}
