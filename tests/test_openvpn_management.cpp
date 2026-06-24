/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Host-side unit tests for the OpenVPN management-interface parser. These are
 * intentionally BeAPI-free so they build and run on any host (Linux, Haiku)
 * with a plain C++ compiler -- see tests/Makefile. They cover the protocol
 * logic that drives the real VPN connection.
 */
#include <stdio.h>

#include <string>
#include <vector>

#include "OpenVPNManagement.h"
#include "VPNState.h"


static int sChecks = 0;
static int sFailures = 0;


static void
check(bool condition, const char* what)
{
	sChecks++;
	if (!condition) {
		sFailures++;
		printf("  FAIL: %s\n", what);
	}
}


static void
check_eq_str(const std::string& actual, const std::string& expected,
	const char* what)
{
	sChecks++;
	if (actual != expected) {
		sFailures++;
		printf("  FAIL: %s (got '%s', expected '%s')\n", what,
			actual.c_str(), expected.c_str());
	}
}


static void
test_state_parsing()
{
	printf("test_state_parsing\n");
	OpenVPNEvent e = OpenVPNManagement::ParseLine(
		">STATE:1700000000,CONNECTED,SUCCESS,10.8.0.2,192.0.2.1,1194,49152");
	check(e.type == OPENVPN_EVENT_STATE, "type is STATE");
	check_eq_str(e.stateName, "CONNECTED", "stateName");
	check_eq_str(e.stateDetail, "SUCCESS", "stateDetail");
	check_eq_str(e.localIP, "10.8.0.2", "localIP");
	check_eq_str(e.remoteIP, "192.0.2.1", "remoteIP");
	check(e.mappedState == VPN_STATE_CONNECTED, "mappedState CONNECTED");
}


static void
test_state_mapping()
{
	printf("test_state_mapping\n");
	check(OpenVPNManagement::StateForName("CONNECTING") == VPN_STATE_CONNECTING,
		"CONNECTING");
	check(OpenVPNManagement::StateForName("RESOLVE") == VPN_STATE_CONNECTING,
		"RESOLVE");
	check(OpenVPNManagement::StateForName("AUTH") == VPN_STATE_AUTHENTICATING,
		"AUTH");
	check(OpenVPNManagement::StateForName("GET_CONFIG")
		== VPN_STATE_AUTHENTICATING, "GET_CONFIG");
	check(OpenVPNManagement::StateForName("CONNECTED") == VPN_STATE_CONNECTED,
		"CONNECTED");
	check(OpenVPNManagement::StateForName("RECONNECTING")
		== VPN_STATE_RECONNECTING, "RECONNECTING");
	check(OpenVPNManagement::StateForName("EXITING") == VPN_STATE_DISCONNECTED,
		"EXITING");
	check(OpenVPNManagement::StateForName("NONSENSE") == VPN_STATE_ERROR,
		"unknown -> ERROR");
}


static void
test_bytecount()
{
	printf("test_bytecount\n");
	OpenVPNEvent e = OpenVPNManagement::ParseLine(">BYTECOUNT:123456,7890");
	check(e.type == OPENVPN_EVENT_BYTECOUNT, "type is BYTECOUNT");
	check(e.bytesIn == 123456ULL, "bytesIn");
	check(e.bytesOut == 7890ULL, "bytesOut");
}


static void
test_password_and_auth()
{
	printf("test_password_and_auth\n");
	OpenVPNEvent need = OpenVPNManagement::ParseLine(
		">PASSWORD:Need 'Auth' username/password");
	check(need.type == OPENVPN_EVENT_PASSWORD_REQUEST, "password request type");
	check_eq_str(need.realm, "Auth", "password realm");

	OpenVPNEvent failed = OpenVPNManagement::ParseLine(
		">PASSWORD:Verification Failed: 'Auth'");
	check(failed.type == OPENVPN_EVENT_AUTH_FAILED, "auth failed type");
	check_eq_str(failed.realm, "Auth", "auth failed realm");
}


static void
test_log_hold_fatal()
{
	printf("test_log_hold_fatal\n");
	OpenVPNEvent log = OpenVPNManagement::ParseLine(
		">LOG:1700000000,I,OpenVPN 2.6.0 starting");
	check(log.type == OPENVPN_EVENT_LOG, "log type");
	check_eq_str(log.message, "OpenVPN 2.6.0 starting", "log message");

	OpenVPNEvent hold = OpenVPNManagement::ParseLine(
		">HOLD:Waiting for hold release:0");
	check(hold.type == OPENVPN_EVENT_HOLD, "hold type");

	OpenVPNEvent fatal = OpenVPNManagement::ParseLine(
		">FATAL:Cannot resolve host address");
	check(fatal.type == OPENVPN_EVENT_FATAL, "fatal type");
	check_eq_str(fatal.message, "Cannot resolve host address", "fatal message");
}


static void
test_command_replies()
{
	printf("test_command_replies\n");
	OpenVPNEvent ok = OpenVPNManagement::ParseLine("SUCCESS: real-time state notification set to ON");
	check(ok.type == OPENVPN_EVENT_SUCCESS, "success type");

	OpenVPNEvent err = OpenVPNManagement::ParseLine("ERROR: unknown command");
	check(err.type == OPENVPN_EVENT_ERROR, "error type");
	check_eq_str(err.message, "unknown command", "error message");
}


static void
test_feed_partial_lines()
{
	printf("test_feed_partial_lines\n");
	OpenVPNManagement mgmt;
	// A chunk that ends mid-line; the partial tail must be buffered.
	std::vector<OpenVPNEvent> first = mgmt.Feed(
		">STATE:1,CONNECTING,,,\r\n>BYTE");
	check(first.size() == 1, "one complete line from first chunk");
	if (first.size() == 1)
		check(first[0].type == OPENVPN_EVENT_STATE, "first is STATE");

	std::vector<OpenVPNEvent> second = mgmt.Feed("COUNT:10,20\n");
	check(second.size() == 1, "buffered line completes on second chunk");
	if (second.size() == 1) {
		check(second[0].type == OPENVPN_EVENT_BYTECOUNT, "second is BYTECOUNT");
		check(second[0].bytesIn == 10ULL, "buffered bytesIn");
	}
}


static void
test_command_builders()
{
	printf("test_command_builders\n");
	check_eq_str(OpenVPNManagement::CommandStateOn(), "state on", "state on");
	check_eq_str(OpenVPNManagement::CommandByteCount(5), "bytecount 5",
		"bytecount");
	check_eq_str(OpenVPNManagement::CommandHoldRelease(), "hold release",
		"hold release");
	check_eq_str(OpenVPNManagement::CommandSignalTerm(), "signal SIGTERM",
		"signal");
	check_eq_str(OpenVPNManagement::CommandPassword("Auth", "secret"),
		"password \"Auth\" \"secret\"", "password plain");
	// A password containing a quote and a backslash must be escaped.
	check_eq_str(OpenVPNManagement::CommandPassword("Auth", "a\"b\\c"),
		"password \"Auth\" \"a\\\"b\\\\c\"", "password escaped");
}


int
main()
{
	printf("== OpenVPN management-interface tests ==\n");
	test_state_parsing();
	test_state_mapping();
	test_bytecount();
	test_password_and_auth();
	test_log_hold_fatal();
	test_command_replies();
	test_feed_partial_lines();
	test_command_builders();

	printf("\n%d checks, %d failures\n", sChecks, sFailures);
	return sFailures == 0 ? 0 : 1;
}
