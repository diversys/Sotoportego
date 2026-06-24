/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef PROFILE_STORE_H
#define PROFILE_STORE_H


#include <vector>

#include <SupportDefs.h>

#include "VPNProfile.h"

class BMessage;
class BPath;


// Persistent profile list owned by the daemon. Profiles are keyed by name:
// Save() upserts (replace if the name already exists, else append), Delete()
// removes by name. Mutations write the whole flattened list back to disk so
// the state on disk and the state in memory never diverge by more than a
// single mutation.
//
// On-disk format is a single flattened BMessage with one nested BMessage
// (kFieldProfile) per profile, stored at:
//   $B_USER_SETTINGS_DIRECTORY/Sotoportego/profiles
class ProfileStore {
public:
								ProfileStore();
								~ProfileStore();

	// Load the persisted list from disk if it exists. Missing file is not
	// an error (the store stays empty). Returns B_OK on success or when
	// the file is absent, an error code on real I/O failures.
			status_t			Load();

	// Insert-or-update `profile` (matched by name). Persists the new list.
	// Empty-name profiles are rejected with B_BAD_VALUE.
			status_t			Save(const VPNProfile& profile);

	// Remove the profile whose name equals `name`. Returns B_OK whether or
	// not the profile existed.
			status_t			Delete(const char* name);

			size_t				Count() const { return fProfiles.size(); }
			const VPNProfile&	At(size_t index) const
									{ return fProfiles[index]; }

	// Fill `into` with one archived VPNProfile per stored profile, added
	// under kFieldProfile. Suitable for use as a kMsgListProfiles payload.
			status_t			ArchiveAll(BMessage* into) const;

private:
			status_t			_WriteToDisk() const;
			status_t			_SettingsPath(BPath* path,
									bool createDir) const;

			std::vector<VPNProfile>	fProfiles;
};


#endif	// PROFILE_STORE_H
