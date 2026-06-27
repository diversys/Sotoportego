/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Country-to-centroid lookup table for VPN server geocoding.
 *
 * Values are approximate population centres rather than geographic centres:
 * placing the JP pin in Tokyo, the US pin near the eastern seaboard, the
 * RU pin near Moscow gives the user a recognisable spread on the world map
 * even when several dozen servers share the same code. Multi-island /
 * multi-territory entries (US, FR, GB, NL) pick the metropolitan capital.
 *
 * Source: Wikipedia population centres / capital cities (2024), rounded to
 * one decimal so the file stays small. Accuracy is good enough for a map
 * sized in degrees, not meters.
 */
#include "CountryCentroids.h"

#include <ctype.h>


namespace {

struct Centroid {
	const char*	iso2;
	float		lat;
	float		lon;
};

// One row per ISO-2 country code. Sorted alphabetically for readability;
// lookup is a linear scan (~250 entries, single comparison each).
static const Centroid kCentroids[] = {
	{ "AD", 42.5f,    1.5f },     // Andorra
	{ "AE", 25.2f,   55.3f },     // United Arab Emirates
	{ "AF", 33.9f,   67.7f },     // Afghanistan
	{ "AG", 17.1f,  -61.8f },     // Antigua and Barbuda
	{ "AI", 18.2f,  -63.1f },     // Anguilla
	{ "AL", 41.1f,   20.1f },     // Albania
	{ "AM", 40.0f,   45.0f },     // Armenia
	{ "AO", -11.2f,  17.9f },     // Angola
	{ "AQ", -82.9f, 135.0f },     // Antarctica
	{ "AR", -34.6f, -58.4f },     // Argentina (Buenos Aires)
	{ "AS", -14.3f, -170.7f },    // American Samoa
	{ "AT", 48.2f,   16.4f },     // Austria (Vienna)
	{ "AU", -33.9f, 151.2f },     // Australia (Sydney)
	{ "AW", 12.5f,  -69.9f },     // Aruba
	{ "AX", 60.2f,   20.0f },     // Åland Islands
	{ "AZ", 40.4f,   49.9f },     // Azerbaijan (Baku)
	{ "BA", 43.9f,   17.7f },     // Bosnia and Herzegovina
	{ "BB", 13.2f,  -59.5f },     // Barbados
	{ "BD", 23.7f,   90.4f },     // Bangladesh (Dhaka)
	{ "BE", 50.8f,    4.4f },     // Belgium (Brussels)
	{ "BF", 12.2f,   -1.6f },     // Burkina Faso
	{ "BG", 42.7f,   23.3f },     // Bulgaria (Sofia)
	{ "BH", 26.0f,   50.6f },     // Bahrain
	{ "BI", -3.4f,   29.9f },     // Burundi
	{ "BJ", 6.4f,    2.4f },      // Benin
	{ "BL", 17.9f,  -62.8f },     // Saint Barthélemy
	{ "BM", 32.3f,  -64.8f },     // Bermuda
	{ "BN", 4.5f,  114.9f },      // Brunei
	{ "BO", -16.5f, -68.1f },     // Bolivia (La Paz)
	{ "BQ", 12.2f,  -68.3f },     // Caribbean Netherlands
	{ "BR", -23.5f, -46.6f },     // Brazil (São Paulo)
	{ "BS", 25.1f,  -77.4f },     // Bahamas
	{ "BT", 27.5f,   89.6f },     // Bhutan
	{ "BV", -54.4f,   3.4f },     // Bouvet Island
	{ "BW", -24.7f,  25.9f },     // Botswana
	{ "BY", 53.9f,   27.6f },     // Belarus (Minsk)
	{ "BZ", 17.5f,  -88.2f },     // Belize
	{ "CA", 43.7f,  -79.4f },     // Canada (Toronto)
	{ "CC", -12.2f,  96.8f },     // Cocos Islands
	{ "CD", -4.3f,   15.3f },     // DR Congo (Kinshasa)
	{ "CF", 4.4f,   18.6f },      // Central African Republic
	{ "CG", -4.3f,   15.3f },     // Congo (Brazzaville)
	{ "CH", 47.4f,    8.5f },     // Switzerland (Zurich)
	{ "CI", 5.3f,   -4.0f },      // Côte d'Ivoire
	{ "CK", -21.2f, -159.8f },    // Cook Islands
	{ "CL", -33.4f, -70.7f },     // Chile (Santiago)
	{ "CM", 3.9f,   11.5f },      // Cameroon
	{ "CN", 39.9f,  116.4f },     // China (Beijing)
	{ "CO", 4.7f,  -74.1f },      // Colombia (Bogota)
	{ "CR", 9.9f,  -84.1f },      // Costa Rica
	{ "CU", 23.1f,  -82.4f },     // Cuba (Havana)
	{ "CV", 14.9f,  -23.5f },     // Cape Verde
	{ "CW", 12.2f,  -69.0f },     // Curaçao
	{ "CX", -10.5f, 105.7f },     // Christmas Island
	{ "CY", 35.2f,   33.4f },     // Cyprus (Nicosia)
	{ "CZ", 50.1f,   14.4f },     // Czechia (Prague)
	{ "DE", 50.1f,    8.7f },     // Germany (Frankfurt)
	{ "DJ", 11.6f,   43.1f },     // Djibouti
	{ "DK", 55.7f,   12.6f },     // Denmark (Copenhagen)
	{ "DM", 15.4f,  -61.4f },     // Dominica
	{ "DO", 18.5f,  -69.9f },     // Dominican Republic
	{ "DZ", 36.8f,    3.1f },     // Algeria (Algiers)
	{ "EC", -0.2f,  -78.5f },     // Ecuador (Quito)
	{ "EE", 59.4f,   24.8f },     // Estonia (Tallinn)
	{ "EG", 30.0f,   31.2f },     // Egypt (Cairo)
	{ "EH", 24.2f,  -12.9f },     // Western Sahara
	{ "ER", 15.3f,   38.9f },     // Eritrea
	{ "ES", 40.4f,   -3.7f },     // Spain (Madrid)
	{ "ET", 9.0f,   38.7f },      // Ethiopia (Addis Ababa)
	{ "FI", 60.2f,   24.9f },     // Finland (Helsinki)
	{ "FJ", -17.7f, 178.1f },     // Fiji
	{ "FK", -51.7f, -59.5f },     // Falkland Islands
	{ "FM", 6.9f,  158.2f },      // Micronesia
	{ "FO", 62.0f,   -6.8f },     // Faroe Islands
	{ "FR", 48.9f,    2.4f },     // France (Paris)
	{ "GA", 0.4f,    9.5f },      // Gabon
	{ "GB", 51.5f,   -0.1f },     // United Kingdom (London)
	{ "GD", 12.1f,  -61.7f },     // Grenada
	{ "GE", 41.7f,   44.8f },     // Georgia (Tbilisi)
	{ "GF", 4.0f,  -52.9f },      // French Guiana
	{ "GG", 49.5f,   -2.6f },     // Guernsey
	{ "GH", 5.6f,   -0.2f },      // Ghana (Accra)
	{ "GI", 36.1f,   -5.4f },     // Gibraltar
	{ "GL", 64.2f,  -51.7f },     // Greenland
	{ "GM", 13.5f,  -16.6f },     // Gambia
	{ "GN", 9.6f,  -13.6f },      // Guinea
	{ "GP", 16.3f,  -61.5f },     // Guadeloupe
	{ "GQ", 3.7f,    8.8f },      // Equatorial Guinea
	{ "GR", 38.0f,   23.7f },     // Greece (Athens)
	{ "GS", -54.4f, -36.6f },     // South Georgia
	{ "GT", 14.6f,  -90.5f },     // Guatemala
	{ "GU", 13.4f,  144.7f },     // Guam
	{ "GW", 12.0f,  -15.6f },     // Guinea-Bissau
	{ "GY", 6.8f,  -58.2f },      // Guyana
	{ "HK", 22.3f,  114.2f },     // Hong Kong
	{ "HM", -53.1f,  73.5f },     // Heard Island
	{ "HN", 14.1f,  -87.2f },     // Honduras
	{ "HR", 45.8f,   16.0f },     // Croatia (Zagreb)
	{ "HT", 18.5f,  -72.3f },     // Haiti
	{ "HU", 47.5f,   19.1f },     // Hungary (Budapest)
	{ "ID", -6.2f,  106.8f },     // Indonesia (Jakarta)
	{ "IE", 53.3f,   -6.3f },     // Ireland (Dublin)
	{ "IL", 31.8f,   35.2f },     // Israel (Jerusalem)
	{ "IM", 54.2f,   -4.5f },     // Isle of Man
	{ "IN", 28.6f,   77.2f },     // India (Delhi)
	{ "IO", -7.3f,   72.4f },     // British Indian Ocean Territory
	{ "IQ", 33.3f,   44.4f },     // Iraq (Baghdad)
	{ "IR", 35.7f,   51.4f },     // Iran (Tehran)
	{ "IS", 64.1f,  -21.9f },     // Iceland (Reykjavik)
	{ "IT", 41.9f,   12.5f },     // Italy (Rome)
	{ "JE", 49.2f,   -2.1f },     // Jersey
	{ "JM", 18.0f,  -76.8f },     // Jamaica
	{ "JO", 31.9f,   35.9f },     // Jordan
	{ "JP", 35.7f,  139.7f },     // Japan (Tokyo)
	{ "KE", -1.3f,   36.8f },     // Kenya (Nairobi)
	{ "KG", 42.9f,   74.6f },     // Kyrgyzstan
	{ "KH", 11.6f,  104.9f },     // Cambodia (Phnom Penh)
	{ "KI", 1.4f,  173.0f },      // Kiribati
	{ "KM", -11.7f,  43.3f },     // Comoros
	{ "KN", 17.3f,  -62.7f },     // Saint Kitts and Nevis
	{ "KP", 39.0f,  125.8f },     // North Korea (Pyongyang)
	{ "KR", 37.6f,  127.0f },     // South Korea (Seoul)
	{ "KW", 29.4f,   48.0f },     // Kuwait
	{ "KY", 19.3f,  -81.4f },     // Cayman Islands
	{ "KZ", 51.2f,   71.4f },     // Kazakhstan (Astana)
	{ "LA", 17.9f,  102.6f },     // Laos
	{ "LB", 33.9f,   35.5f },     // Lebanon (Beirut)
	{ "LC", 14.0f,  -61.0f },     // Saint Lucia
	{ "LI", 47.1f,    9.5f },     // Liechtenstein
	{ "LK", 6.9f,   79.9f },      // Sri Lanka (Colombo)
	{ "LR", 6.3f,  -10.8f },      // Liberia
	{ "LS", -29.6f,  27.5f },     // Lesotho
	{ "LT", 54.7f,   25.3f },     // Lithuania (Vilnius)
	{ "LU", 49.6f,    6.1f },     // Luxembourg
	{ "LV", 56.9f,   24.1f },     // Latvia (Riga)
	{ "LY", 32.9f,   13.2f },     // Libya (Tripoli)
	{ "MA", 33.6f,   -7.6f },     // Morocco (Casablanca)
	{ "MC", 43.7f,    7.4f },     // Monaco
	{ "MD", 47.0f,   28.9f },     // Moldova
	{ "ME", 42.4f,   19.3f },     // Montenegro
	{ "MF", 18.1f,  -63.1f },     // Saint Martin
	{ "MG", -18.9f,  47.5f },     // Madagascar
	{ "MH", 7.1f,  171.4f },      // Marshall Islands
	{ "MK", 42.0f,   21.4f },     // North Macedonia
	{ "ML", 12.7f,   -8.0f },     // Mali
	{ "MM", 16.8f,   96.2f },     // Myanmar (Yangon)
	{ "MN", 47.9f,  106.9f },     // Mongolia (Ulaanbaatar)
	{ "MO", 22.2f,  113.5f },     // Macau
	{ "MP", 15.2f,  145.7f },     // Northern Mariana Islands
	{ "MQ", 14.6f,  -61.0f },     // Martinique
	{ "MR", 18.1f,  -16.0f },     // Mauritania
	{ "MS", 16.7f,  -62.2f },     // Montserrat
	{ "MT", 35.9f,   14.4f },     // Malta (Valletta)
	{ "MU", -20.3f,  57.5f },     // Mauritius
	{ "MV", 4.2f,   73.5f },      // Maldives
	{ "MW", -13.9f,  33.8f },     // Malawi
	{ "MX", 19.4f,  -99.1f },     // Mexico (Mexico City)
	{ "MY", 3.1f,  101.7f },      // Malaysia (Kuala Lumpur)
	{ "MZ", -25.9f,  32.6f },     // Mozambique
	{ "NA", -22.6f,  17.1f },     // Namibia
	{ "NC", -22.3f, 166.5f },     // New Caledonia
	{ "NE", 13.5f,    2.1f },     // Niger
	{ "NF", -29.0f, 167.9f },     // Norfolk Island
	{ "NG", 9.1f,    7.5f },      // Nigeria (Abuja)
	{ "NI", 12.1f,  -86.3f },     // Nicaragua
	{ "NL", 52.4f,    4.9f },     // Netherlands (Amsterdam)
	{ "NO", 59.9f,   10.7f },     // Norway (Oslo)
	{ "NP", 27.7f,   85.3f },     // Nepal (Kathmandu)
	{ "NR", -0.5f,  166.9f },     // Nauru
	{ "NU", -19.1f, -169.9f },    // Niue
	{ "NZ", -36.8f, 174.8f },     // New Zealand (Auckland)
	{ "OM", 23.6f,   58.5f },     // Oman
	{ "PA", 9.0f,  -79.5f },      // Panama (Panama City)
	{ "PE", -12.0f, -77.0f },     // Peru (Lima)
	{ "PF", -17.5f, -149.6f },    // French Polynesia
	{ "PG", -9.5f,  147.2f },     // Papua New Guinea
	{ "PH", 14.6f,  121.0f },     // Philippines (Manila)
	{ "PK", 33.7f,   73.1f },     // Pakistan (Islamabad)
	{ "PL", 52.2f,   21.0f },     // Poland (Warsaw)
	{ "PM", 46.8f,  -56.2f },     // Saint Pierre
	{ "PN", -25.1f, -130.1f },    // Pitcairn
	{ "PR", 18.5f,  -66.1f },     // Puerto Rico
	{ "PS", 31.9f,   35.2f },     // Palestine
	{ "PT", 38.7f,   -9.1f },     // Portugal (Lisbon)
	{ "PW", 7.5f,  134.6f },      // Palau
	{ "PY", -25.3f, -57.6f },     // Paraguay
	{ "QA", 25.3f,   51.5f },     // Qatar (Doha)
	{ "RE", -20.9f,  55.5f },     // Réunion
	{ "RO", 44.4f,   26.1f },     // Romania (Bucharest)
	{ "RS", 44.8f,   20.5f },     // Serbia (Belgrade)
	{ "RU", 55.8f,   37.6f },     // Russia (Moscow)
	{ "RW", -1.9f,   30.1f },     // Rwanda
	{ "SA", 24.7f,   46.7f },     // Saudi Arabia (Riyadh)
	{ "SB", -9.6f,  160.2f },     // Solomon Islands
	{ "SC", -4.7f,   55.5f },     // Seychelles
	{ "SD", 15.6f,   32.5f },     // Sudan (Khartoum)
	{ "SE", 59.3f,   18.1f },     // Sweden (Stockholm)
	{ "SG", 1.4f,  103.8f },      // Singapore
	{ "SH", -15.9f,  -5.7f },     // Saint Helena
	{ "SI", 46.1f,   14.5f },     // Slovenia (Ljubljana)
	{ "SJ", 78.2f,   15.6f },     // Svalbard
	{ "SK", 48.2f,   17.1f },     // Slovakia (Bratislava)
	{ "SL", 8.5f,  -13.2f },      // Sierra Leone
	{ "SM", 43.9f,   12.5f },     // San Marino
	{ "SN", 14.7f,  -17.5f },     // Senegal (Dakar)
	{ "SO", 2.0f,   45.3f },      // Somalia
	{ "SR", 5.8f,  -55.2f },      // Suriname
	{ "SS", 4.9f,   31.6f },      // South Sudan
	{ "ST", 0.3f,    6.7f },      // São Tomé and Príncipe
	{ "SV", 13.7f,  -89.2f },     // El Salvador
	{ "SX", 18.0f,  -63.1f },     // Sint Maarten
	{ "SY", 33.5f,   36.3f },     // Syria (Damascus)
	{ "SZ", -26.3f,  31.1f },     // Eswatini
	{ "TC", 21.7f,  -71.8f },     // Turks and Caicos
	{ "TD", 12.1f,   15.0f },     // Chad
	{ "TF", -49.3f,  69.3f },     // French Southern Territories
	{ "TG", 6.1f,    1.2f },      // Togo
	{ "TH", 13.8f,  100.5f },     // Thailand (Bangkok)
	{ "TJ", 38.6f,   68.8f },     // Tajikistan
	{ "TK", -9.2f, -171.8f },     // Tokelau
	{ "TL", -8.6f,  125.6f },     // East Timor
	{ "TM", 37.9f,   58.4f },     // Turkmenistan
	{ "TN", 36.8f,   10.2f },     // Tunisia (Tunis)
	{ "TO", -21.1f, -175.2f },    // Tonga
	{ "TR", 41.0f,   28.9f },     // Turkey (Istanbul)
	{ "TT", 10.7f,  -61.5f },     // Trinidad and Tobago
	{ "TV", -8.5f,  179.2f },     // Tuvalu
	{ "TW", 25.0f,  121.6f },     // Taiwan (Taipei)
	{ "TZ", -6.8f,   39.3f },     // Tanzania (Dar es Salaam)
	{ "UA", 50.5f,   30.5f },     // Ukraine (Kyiv)
	{ "UG", 0.3f,   32.6f },      // Uganda (Kampala)
	{ "UM", 19.3f, 166.6f },      // US Minor Outlying Islands
	{ "US", 38.9f,  -77.0f },     // United States (Washington DC)
	{ "UY", -34.9f, -56.2f },     // Uruguay (Montevideo)
	{ "UZ", 41.3f,   69.3f },     // Uzbekistan
	{ "VA", 41.9f,   12.5f },     // Vatican
	{ "VC", 13.2f,  -61.2f },     // Saint Vincent
	{ "VE", 10.5f,  -66.9f },     // Venezuela (Caracas)
	{ "VG", 18.4f,  -64.6f },     // British Virgin Islands
	{ "VI", 18.3f,  -64.9f },     // US Virgin Islands
	{ "VN", 21.0f,  105.9f },     // Vietnam (Hanoi)
	{ "VU", -17.7f, 168.3f },     // Vanuatu
	{ "WF", -13.3f, -176.2f },    // Wallis and Futuna
	{ "WS", -13.8f, -172.1f },    // Samoa
	{ "XK", 42.7f,   21.2f },     // Kosovo
	{ "YE", 15.4f,   44.2f },     // Yemen (Sana'a)
	{ "YT", -12.8f,  45.2f },     // Mayotte
	{ "ZA", -26.2f,  28.0f },     // South Africa (Johannesburg)
	{ "ZM", -15.4f,  28.3f },     // Zambia
	{ "ZW", -17.8f,  31.1f }      // Zimbabwe (Harare)
};
static const size_t kCentroidCount = sizeof(kCentroids) / sizeof(kCentroids[0]);

}	// namespace


bool
CountryCentroids::Lookup(const BString& iso2, float& latOut, float& lonOut)
{
	BString code(iso2);
	code.Trim();
	if (code.Length() < 2)
		return false;

	// Upper-case the first two characters in place.
	char key[3] = { 0, 0, 0 };
	key[0] = (char)toupper((unsigned char)code.ByteAt(0));
	key[1] = (char)toupper((unsigned char)code.ByteAt(1));

	for (size_t i = 0; i < kCentroidCount; i++) {
		if (kCentroids[i].iso2[0] == key[0]
				&& kCentroids[i].iso2[1] == key[1]) {
			latOut = kCentroids[i].lat;
			lonOut = kCentroids[i].lon;
			return true;
		}
	}
	return false;
}
