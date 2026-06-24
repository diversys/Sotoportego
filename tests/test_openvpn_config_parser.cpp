/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Host-side unit tests for OpenVPNConfigParser. Run on any host with g++.
 */
#include "OpenVPNConfigParser.h"
#include "VPNProfile.h"

#include <stdio.h>
#include <string.h>


static int g_failed = 0;


static void
expect_eq_str(const char* label, const BString& got, const char* expected)
{
	if (got != expected) {
		fprintf(stderr, "FAIL %s: expected '%s', got '%s'\n",
			label, expected, got.String());
		g_failed++;
	}
}


static void
expect_eq_u16(const char* label, uint16 got, uint16 expected)
{
	if (got != expected) {
		fprintf(stderr, "FAIL %s: expected %u, got %u\n",
			label, (unsigned)expected, (unsigned)got);
		g_failed++;
	}
}


static void
test_minimal_remote()
{
	VPNProfile profile;
	OpenVPNConfigParser::ParseText(
		"client\n"
		"remote vpn.example.com\n",
		profile);
	expect_eq_str("minimal/server", profile.fServer, "vpn.example.com");
	expect_eq_u16("minimal/port", profile.fPort, 1194);
	expect_eq_str("minimal/protocol", profile.fProtocol, "udp");
}


static void
test_remote_with_port()
{
	VPNProfile profile;
	OpenVPNConfigParser::ParseText(
		"remote vpn.example.com 443\n"
		"proto tcp\n",
		profile);
	expect_eq_str("portTcp/server", profile.fServer, "vpn.example.com");
	expect_eq_u16("portTcp/port", profile.fPort, 443);
	expect_eq_str("portTcp/protocol", profile.fProtocol, "tcp");
}


static void
test_explicit_port_directive_overrides_remote()
{
	VPNProfile profile;
	// `port` after `remote` should override the port carried by `remote`.
	OpenVPNConfigParser::ParseText(
		"remote vpn.example.com 1194\n"
		"port 8443\n",
		profile);
	expect_eq_u16("portOverride/port", profile.fPort, 8443);
}


static void
test_remote_with_inline_proto()
{
	VPNProfile profile;
	OpenVPNConfigParser::ParseText(
		"remote vpn.example.com 1194 tcp-client\n",
		profile);
	expect_eq_str("remoteInline/protocol", profile.fProtocol, "tcp");
}


static void
test_proto_variants_map_to_tcp_or_udp()
{
	struct { const char* proto; const char* expected; } cases[] = {
		{ "tcp",        "tcp" },
		{ "tcp-client", "tcp" },
		{ "tcp4",       "tcp" },
		{ "udp",        "udp" },
		{ "udp6",       "udp" },
		{ NULL,         NULL }
	};
	for (int i = 0; cases[i].proto != NULL; i++) {
		VPNProfile profile;
		char buf[64];
		snprintf(buf, sizeof(buf), "proto %s\n", cases[i].proto);
		OpenVPNConfigParser::ParseText(buf, profile);
		expect_eq_str(cases[i].proto, profile.fProtocol, cases[i].expected);
	}
}


static void
test_comments_and_blanks_are_ignored()
{
	VPNProfile profile;
	OpenVPNConfigParser::ParseText(
		"# this is a comment\n"
		"\n"
		"; semicolons are comments too\n"
		"  remote vpn.example.com 1194  # trailing comment\n"
		"\n"
		"proto udp ; another trailing\n",
		profile);
	expect_eq_str("comments/server", profile.fServer, "vpn.example.com");
	expect_eq_u16("comments/port", profile.fPort, 1194);
	expect_eq_str("comments/protocol", profile.fProtocol, "udp");
}


static void
test_empty_text_yields_defaults()
{
	VPNProfile profile;
	OpenVPNConfigParser::ParseText("", profile);
	expect_eq_str("empty/server", profile.fServer, "");
	expect_eq_u16("empty/port", profile.fPort, 1194);
	expect_eq_str("empty/protocol", profile.fProtocol, "udp");
}


int
main()
{
	test_minimal_remote();
	test_remote_with_port();
	test_explicit_port_directive_overrides_remote();
	test_remote_with_inline_proto();
	test_proto_variants_map_to_tcp_or_udp();
	test_comments_and_blanks_are_ignored();
	test_empty_text_yields_defaults();

	if (g_failed == 0) {
		printf("OpenVPNConfigParser: all tests passed\n");
		return 0;
	}
	printf("OpenVPNConfigParser: %d failure(s)\n", g_failed);
	return 1;
}
