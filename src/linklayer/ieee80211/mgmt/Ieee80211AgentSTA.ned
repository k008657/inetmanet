//
// Copyright (C) 2006 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//


package inet.linklayer.ieee80211.mgmt;

//
// Used in 802.11 infrastructure mode: in a station (STA), this module
// controls channel scanning, association and handovers, by sending commands
// (e.g. Ieee80211Prim_ScanRequest) to the management module (Ieee80211MgmtSTA).
//
// @author Andras Varga
//
// Modified by Juan-Carlos Maureira. INRIA 2009
// - correct the dissasociation and back to scanning behavior
// - add a fixed ssid to connect with.
// - Agent status logging vector.
// - Agent starting time random. (avoid all the STAs starts to scan at the same time)
//
//
simple Ieee80211AgentSTA
{
    parameters:
        bool activeScan = default(true);  // selects between active and passive scanning
        string channelsToScan = default("");  // list of channel numbers to scan (space delimited); empty means all channels
        double probeDelay @unit("s") = default(0.1s); // delay before sending a probe request during active scanning
        double minChannelTime @unit("s") = default(0.15s); // min interval to spend on a channel during active scanning
        double maxChannelTime @unit("s") = default(0.3s); // channel time for passive scanning, and max channel time for active scanning
        double authenticationTimeout @unit("s") = default(5s); // timeout for the authentication procedure
        double associationTimeout @unit("s") = default(5s); // timeout for the association procedure
        double startingTime @unit("s") = default(uniform(0s,5s)); // agent starting time.
        string default_ssid            = default(""); // default ssid
 		@display("i=block/control");
    gates:
        input mgmtIn;
        output mgmtOut;
}
