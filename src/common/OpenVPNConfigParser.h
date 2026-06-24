/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef OPENVPN_CONFIG_PARSER_H
#define OPENVPN_CONFIG_PARSER_H


#include <string>

#include "VPNProfile.h"


// Minimal OpenVPN client-config parser.
//
// OpenVPN's configuration grammar is rich (push directives, inline blocks,
// pkcs#12 references, ...). For now we only need enough of it to populate
// the fields VPNProfile cares about so the GUI can show what a profile
// will end up connecting to:
//
//   * remote <host> [port]
//   * proto  <udp|tcp(-client)|udp4|tcp4|...>
//   * port   <port>     (overrides the port on `remote` if present)
//   * auth-user-pass    (records that interactive credentials are needed)
//
// Anything else is preserved indirectly by saving the path to the .ovpn
// file in fConfigPath; when the real openvpn process gets launched it will
// be invoked with `--config <path>` and honour the full grammar itself.
//
// The parser is intentionally BeAPI-free (std::string in/out) so it can be
// unit-tested on any host. The wrappers in the cpp accept either a file
// path or already-loaded text.
class OpenVPNConfigParser {
public:
	// Parse the text of an .ovpn config. fName/fConfigPath are NOT set by
	// this method (the caller picks how to derive them).
	static	void				ParseText(const std::string& text,
									VPNProfile& profile);

	// Convenience: read the file at `path` and parse its contents. Also
	// sets profile.fConfigPath = path and derives a default fName from the
	// file's basename (without extension). Returns true on success.
	static	bool				ParseFile(const char* path,
									VPNProfile& profile);
};


#endif	// OPENVPN_CONFIG_PARSER_H
