/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * MapView -- pan/zoom world map with VPN server pins. Adapted from the
 * Sestriere project (also by atomozero), with the Meshtastic-specific
 * contact / SAR-marker machinery stripped out.
 */
#ifndef MAP_VIEW_H
#define MAP_VIEW_H


#include <View.h>

#include "Compat.h"

class BBitmap;
class TileCache;


// Toolbar message codes; wired by the host window to the buttons it
// surfaces.
enum {
	kMsgZoomIn			= 'mvZi',
	kMsgZoomOut			= 'mvZo',
	kMsgZoomFit			= 'mvZf',
	kMsgCenterSelf		= 'mvCs',
	kMsgToggleTiles		= 'mvTt',
	kMsgDownloadArea	= 'mvDa',
	// Sent to the host window when the user clicks a server pin. Carries
	// kFieldHost (string).
	kMsgServerSelected	= 'mvSs'
};


// BMessage field name used by kMsgServerSelected.
extern const char* const kFieldHost;


// One VPN server placed on the map. The host string is the canonical
// identifier; the rest is presentation/sort metadata that the side panel
// shows when the pin is selected.
struct ServerPin {
	BString		host;
	BString		countryShort;	// ISO-2 code, e.g. "JP"
	BString		countryLong;	// human name, e.g. "Japan"
	BString		logPolicy;		// vpngate LogType field
	float		latitude;
	float		longitude;
	int32		score;			// vpngate score, higher = better
	int32		pingMs;
	int32		speedMbps;
	int32		sessions;
	bool		selected;

	ServerPin()
		:
		latitude(0), longitude(0),
		score(0), pingMs(0), speedMbps(0), sessions(0),
		selected(false)
	{
	}
};


class MapView : public BView {
public:
								MapView(const char* name);
	virtual						~MapView();

	virtual	void				AttachedToWindow();
	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				MouseUp(BPoint where);
	virtual	void				FrameResized(float newWidth, float newHeight);
	virtual	void				MessageReceived(BMessage* message);

	// Pin set management. AddServer keys by host; passing the same host
	// twice replaces the previous entry.
			void				AddServer(const ServerPin& pin);
			void				ClearServers();
			void				SetSelectedHost(const BString& host);
			const ServerPin*	SelectedServer() const;

	// "You are here" marker (currently unused -- left as the seam for a
	// future "look up my public IP" hookup).
			void				SetSelfPosition(float lat, float lon,
									const char* label);
			void				ClearSelf();

			void				ZoomIn();
			void				ZoomOut();
			void				ZoomToFit();
			void				CenterOnSelf();

			void				SetTilesEnabled(bool enabled);
			bool				TilesEnabled() const { return fShowTiles; }

			void				SaveMapState();
			void				LoadMapState();
			void				DownloadVisibleArea();

private:
	// Mercator projection helpers.
	static	float				_MercatorY(float latDeg);
			BPoint				_LatLonToScreen(float lat, float lon) const;
			void				_ScreenToLatLon(BPoint screen, float& lat,
									float& lon) const;

			void				_DrawTiles();
			void				_DrawCoastlines();
			void				_DrawGrid();
			void				_DrawPins();
			void				_DrawPin(const ServerPin& pin);
			void				_DrawSelf();
			void				_DrawScaleBar();
			void				_DrawCompass();

			ServerPin*			_FindPinAt(BPoint where);
			int					_ZoomToTileZoom() const;

			OwningObjectList<ServerPin>	fPins;

			bool				fHasSelfPosition;
			float				fSelfLat;
			float				fSelfLon;
			BString				fSelfLabel;

			float				fCenterLat;
			float				fCenterLon;
			float				fZoom;		// pixels per degree longitude

			bool				fDragging;
			bool				fDragMoved;
			BPoint				fDragStart;
			float				fDragStartLat;
			float				fDragStartLon;

			ServerPin*			fSelectedPin;
			ServerPin*			fHoverPin;

			TileCache*			fTileCache;
			bool				fShowTiles;
			bool				fShowCoastlines;

			int					fLastTileZ;
			int					fLastTileMinX;
			int					fLastTileMinY;
			int					fLastTileMaxX;
			int					fLastTileMaxY;
};


#endif	// MAP_VIEW_H
