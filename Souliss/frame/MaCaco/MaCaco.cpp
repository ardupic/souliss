/**************************************************************************
	Souliss - MaCaco Communication Protocol
    Copyright (C) 2011  Veseo

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
	
	Originally developed by Dario Di Maio
	
***************************************************************************/
/*!
    \file 
    \ingroup

*/
/**************************************************************************/

#include <stdio.h>
#include <string.h>

#include "frame/vNet/vNet.h"
#include "frame/MaCaco/MaCaco.h"
#include "GetConfig.h"								// need : MaCaco_Config.h

U8 MaCaco_data[MaCaco_FRAME] = {0x00}; 				// Data buffer for incoming data
MaCaco_rx_data_t MaCaco_rx;							// Data structure

U8 MaCaco_header[MaCaco_HEADER]  = {0x00};			// Header for output frame				
oFrame Macaco_oFrame;								// Data structure for output frame (local data)
oFrame MacacoUserMode_oFrame;						// Data structure for output frame (data from other nodes)

// store incoming subscription state
U16 subscr_addr[MaCaco_INMAXSUBSCR] = {0x0000};	
U8* subscr_putin[MaCaco_INMAXSUBSCR] = {0x0000};
U8 subscr_startoffset[MaCaco_INMAXSUBSCR] = {0x00};
U8 subscr_numberof[MaCaco_INMAXSUBSCR] = {0x00};
U8 subscr_funcode[MaCaco_INMAXSUBSCR] = {0x00};

// store outgoing subscription state
U16 subscr_outaddr[MaCaco_OUTMAXSUBSCR] = {0x0000};	// Record the address of subscribed nodes	
bool subscr_status[MaCaco_OUTMAXSUBSCR] = {0};		// Flag if last incoming data was a subscription
bool subscr_battery[MaCaco_OUTMAXSUBSCR] = {0};		// Flag battery operated devices	
U8 subscr_count[MaCaco_OUTMAXSUBSCR] = {0x00};

// store incoming typical logic request
U16 reqtyp_addr = 0x0000;	
U8* reqtyp_putin = 0x0000;
U8 reqtyp_startoffset = 0x00;
U8 reqtyp_numberof = 0x00;
U8 reqtyp_times = 0;

// store incoming addressing request information
U16 randomkeyid = 0;
U16 proposedaddress = 0;

// buffer for temporary use
U8 ipaddrs[4], cmd[7] = {0, MaCaco_NODES, MaCaco_SLOT, MaCaco_INMAXSUBSCR, MaCaco_IN_s, MaCaco_TYP_s, MaCaco_OUT_s};

#if (MaCaco_DEBUG)
	#define MaCaco_LOG Serial.print
#endif

/**************************************************************************/
/*!
    Send MaCaco frame over the vNet
*/
/**************************************************************************/
U8 MaCacoTX(U16 addr, oFrame* Macaco_oFrame, U8 len)
{
	#if (MaCaco_DEBUG)
	// Print the outgoing message header
	MaCaco_LOG("(MaCaco)<OUT>");
	MaCaco_LOG("<|0x");
	for(U8 i=0;i<MaCaco_HEADER;i++)
	{
		if((i!=1) || (i!=2) || (i!=4))
		{
			MaCaco_LOG(Macaco_oFrame->header[i],HEX);
			MaCaco_LOG("|0x");
		}	
		else if((i==1))
		{
			MaCaco_LOG((U16)Macaco_oFrame->header[i],HEX);
			MaCaco_LOG("|0x");
		}	
		else if((i==5))
			MaCaco_LOG(Macaco_oFrame->header[i],HEX);
	}		
	MaCaco_LOG(">\r\n");
	#endif	
		
	return vNet_Send(addr, Macaco_oFrame, len, MaCaco_PORT); 
}
/**************************************************************************/
/*!
    Parse the input data and check for functional code consistency
*/
/**************************************************************************/
U8 MaCaco_parse(MaCaco_rx_data_t *rx)
{
    U8 i=0, len, func, *data_ptr;

    data_ptr = rx->data;

	#if (MaCaco_DEBUG)
	// Print the outgoing message header
	MaCaco_LOG("(MaCaco)<IN><|0x");
	for(U8 i=0;i<MaCaco_HEADER;i++)
	{
		if((i!=1) || (i!=2) || (i!=4))
			{
				MaCaco_LOG(data_ptr[i],HEX);
				MaCaco_LOG("|0x");
			}	
		else if((i==1))
			{
				MaCaco_LOG((U16)data_ptr[i],HEX);
				MaCaco_LOG("|0x");
			}	
		else if((i==5))
			MaCaco_LOG(data_ptr[i],HEX);
	}		
	MaCaco_LOG(">\r\n");
	#endif	
	
   // parse the buffer and extract funcode, putin and numberof
   
	rx->funcode = *data_ptr++;				// Functional Code
	func = rx->funcode;
	
	// Read the value as 16 bit and convert it to pointer for an 8 bit variable
	rx->putin = (U8 *)(*(U16 *)data_ptr);	// Put in...
	data_ptr += sizeof(U16);
	
	rx->startoffset = *data_ptr++;			// Start offset

	rx->numberof = *data_ptr++;				// Numer of
	len = rx->numberof;	
	
	// if is an "answer" functional code
	if (((func & 0xF0) == 0x10) || ((func & 0xF0) == 0x30) || ((func & 0xF0) == 0x50) || ((func & 0xF0) == 0x70))
		memmove(rx->data, data_ptr, len); // Remove message header from data
		
	// check if functional code is supported	
	while ((MaCaco_funcode[i] != func) && (i < MaCaco_FUNCODE_NO - 1))	
		i++;
	
	if ((MaCaco_funcode[i] == func))
		return func;				// functional code supported
	else	
		return MaCaco_FUNCODE_ERR;	// functional code not supported

}

/**************************************************************************/
/*!
    Build frame and write to destination device, it use the oFrame structure
*/
/**************************************************************************/
U8 MaCaco_send(U16 addr, U8 funcode, U8 *putin, U8 startoffset, U8 numberof, U8 *data)
{
	U8 *frame_pnt, *data_pnt, status;
	
	frame_pnt = &MaCaco_header[0];		// init the pointer
	data_pnt = data;					// init the pointer
	
	// Build frame header
	*frame_pnt++ = funcode;
	
	// Read the value as 16 bit and convert the pointer for an 16 bit variable
	*(U16 *)frame_pnt = (U16) putin;
	frame_pnt += sizeof(U16);
	
	*frame_pnt++ = startoffset;	
	*frame_pnt++ = numberof;
	
	// Define the output frame structure
	oFrame_Define(&Macaco_oFrame);
	
	// if is an "answer" functional code
	if (((funcode & 0xF0) == 0x10) || ((funcode & 0xF0) == 0x30) || ((funcode & 0xF0) == 0x50) || ((funcode & 0xF0) == 0x70))
	{
		// Set the header and data payload in the frame
		oFrame_Set(MaCaco_header, data_pnt, MaCaco_HEADER, numberof, 0);

		// Write to destination device
		status = MaCacoTX(addr, &Macaco_oFrame, numberof + MaCaco_HEADER);
	}
	else
	{
		oFrame_Set(MaCaco_header, 0, MaCaco_HEADER, 0, 0);
		status = MaCacoTX(addr, &Macaco_oFrame, MaCaco_HEADER);
	}	
		
	return status;
}

/**************************************************************************/
/*!
    Build frame and write to destination device in case of User Mode,
	it use the oFrame structure
*/
/**************************************************************************/
U8 MaCacoUserMode_send(U16 addr, U8 funcode, U8 *putin, U8 startoffset, U8 numberof, U8 l_numberof, U8 u_numberof, U8 *l_data, U8 *u_data)
{
	U8 *frame_pnt, *l_data_pnt, *u_data_pnt, status;
	
	frame_pnt = &MaCaco_header[0];		// Init the pointer for the frame header
	l_data_pnt = l_data;				// Init the pointer for the local data
	u_data_pnt = u_data;				// Init the pointer for the data from other nodes
	
	// Build frame header
	*frame_pnt++ = funcode;
	
	// Read the value as 16 bit and convert the pointer for an 16 bit variable
	*(U16 *)frame_pnt = (U16) putin;
	frame_pnt += sizeof(U16);
	
	*frame_pnt++ = startoffset;	
	*frame_pnt++ = numberof;
		
	// if is an "answer" functional code
	if (((funcode & 0xF0) == 0x10) || ((funcode & 0xF0) == 0x30) || ((funcode & 0xF0) == 0x50) || ((funcode & 0xF0) == 0x70))
	{
		// Define the output frame structure for data from nodes in the network
		oFrame_Define(&MacacoUserMode_oFrame);
	
		// Set the data payload in the frame, these are relevant to data from other nodes in the network
		oFrame_Set(0, u_data_pnt, 0, u_numberof, 0);

		// Define the output frame structure for local data
		oFrame_Define(&Macaco_oFrame);
		
		// Set the header and payload, these are relevant to data from local node
		oFrame_Set(MaCaco_header, l_data_pnt, MaCaco_HEADER, l_numberof, &MacacoUserMode_oFrame);

		// Write to destination device
		status = MaCacoTX(addr, &Macaco_oFrame, l_numberof + u_numberof + MaCaco_HEADER);
	}
	else
	{
		// Define the output frame structure for local data
		oFrame_Define(&Macaco_oFrame);
		
		// Send only the header
		oFrame_Set(MaCaco_header, 0, MaCaco_HEADER, 0, 0);
		status = MaCacoTX(addr, &Macaco_oFrame, MaCaco_HEADER);
	}	
		
	return status;
}

/**************************************************************************/
/*!
    Analyze received data, send answer and move data to shared memory map
*/
/**************************************************************************/
U8 MaCaco_peruse(U16 addr, MaCaco_rx_data_t *rx, U8 *memory_map)
{
	U8 i=0, typ_mask=0xFF;
	
	/*********** Request ************/	
	
	// answer to a digital read request
	if ((rx->funcode == MaCaco_READREQDIG))
		return MaCaco_send(addr, MaCaco_READANSDIG, rx->putin, rx->startoffset, rx->numberof, (rx->startoffset + memory_map));

	// answer to a analog read request
	if ((rx->funcode == MaCaco_READREQANA))
		return MaCaco_send(addr, MaCaco_READANSANA, rx->putin, rx->startoffset, rx->numberof, (rx->startoffset + memory_map));

	// answer to a typical logic read request
	if ((rx->funcode == MaCaco_TYPREQ))
	{
			U16 nodeoffest, len;
			
			// These points the local data
			nodeoffest = MaCaco_TYP_s;
			len = MaCaco_TYPLENGHT;				
		
			// Record the request info
			reqtyp_addr = addr;	
			reqtyp_putin = rx->putin;
			reqtyp_startoffset = rx->startoffset;
			reqtyp_numberof = rx->numberof;
				
			// Flag that the request shall be processed for all nodes, this is used at an upper level
			reqtyp_times = MaCaco_NODES;		
			
			// In passthrough mode data from other nodes are not stored, at this time we can send out only local
			// data, then when other nodes will send back data these will be bridged to the User Interface
			if(rx->putin == 0) 
				return MaCaco_send(addr, MaCaco_TYPANS, rx->putin, rx->startoffset, len, (nodeoffest + memory_map));					
	}
		
	// answer to a subscription request
	if ((rx->funcode == MaCaco_SUBSCRREQ) || (rx->funcode == MaCaco_STATEREQ))
	{	
		// look for the subscription channel
		i = 0;
		while((subscr_addr[i] != addr) && (subscr_addr[i] != 0x0000) && (i < MaCaco_INMAXSUBSCR))
			i++;
			
		// Store the subscription data
		if(i < MaCaco_INMAXSUBSCR)
		{
			subscr_addr[i]  = addr;
			subscr_funcode[i] = rx->funcode;
			subscr_putin[i] = rx->putin;
			subscr_startoffset[i] = rx->startoffset;
			subscr_numberof[i] = rx->numberof;
			
		#if(MaCaco_USERMODE)	
			// If the number of nodes wasn't specified
			if(!(rx->numberof) && (rx->funcode == MaCaco_STATEREQ))
			{
				// Count the number of nodes
				U8 nodes = 0;	
				while(((*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*nodes)) != 0x0000) && nodes < MaCaco_NODES)
					nodes++;
					
				subscr_numberof[i] = nodes;	
			}
		#endif
		
		}
		else
			return MaCaco_ERR85;
				
		// answer to subscription
		if(rx->funcode == MaCaco_SUBSCRREQ)
			return MaCaco_send(addr, MaCaco_SUBSCRANS, rx->putin, rx->startoffset, rx->numberof, (rx->startoffset + memory_map));
		else if(rx->funcode == MaCaco_STATEREQ)
		{
			U16 nodeoffest, len;
			
			// Force an update request to refresh data
			for(U8 i=0;i<MaCaco_OUTMAXSUBSCR;i++)
				subscr_count[i] = 0;
			
			// These points the local data
			nodeoffest = MaCaco_OUT_s;
			len = MaCaco_OUTLENGHT;			
			
			// The nodes contains only local data, send them
			if(rx->startoffset == MaCaco_LOCNODE)
				return MaCaco_send(addr, MaCaco_STATEANS, rx->putin, rx->startoffset, len, (nodeoffest + memory_map));
		}
	}

	// answer to a data request without subscription
	if (rx->funcode == MaCaco_DATAREQ)
	{	
		U16 nodeoffest, len;
		
		// If a manual update is requested, force an update request to the remote nodes (if any)
		// to get at next process fresh data
		for(U8 i=0;i<MaCaco_OUTMAXSUBSCR;i++)
			subscr_count[i] = 0;
				
		// These points the local data
		nodeoffest = MaCaco_OUT_s;
		len = MaCaco_OUTLENGHT;			
			
		// The nodes contains only local data, data from other nodes will came with the subscription reset
		// that we have done at begin of this case
		if(rx->startoffset == MaCaco_LOCNODE)
			return MaCaco_send(addr, MaCaco_DATAANS, rx->putin, rx->startoffset, len, (nodeoffest + memory_map));	
	}
	
	// answer to a node healty request
	if (rx->funcode == MaCaco_HEALTYREQ)	
		return MaCaco_send(addr, MaCaco_HEALTYANS, rx->putin, rx->startoffset, rx->numberof, (memory_map + MaCaco_HEALTY_s + rx->startoffset));
	
	// answer to a ping request
	if (rx->funcode == MaCaco_PINGREQ)
		return MaCaco_send(addr, MaCaco_PINGANS, rx->putin, 0x00, 0x00, 0x00);
	
	// answer to a database structure request
	if (rx->funcode == MaCaco_DBSTRUCTREQ)
	{		
		// Count the number of nodes
		U8 nodes = 0;	
		#if(MaCaco_USERMODE)	
		while(((*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*nodes)) != 0x0000) && nodes < MaCaco_NODES)
			nodes++;
		#endif
		
		// Add the actual number of nodes on the database structure frame
		cmd[0] = nodes;		

		if(rx->numberof >= 0x07)
			rx->numberof = 0x07;													// Never send more than 7 bytes of data
		else
			rx->numberof = 0x04;													// Otherwise send the short frame with only 4 bytes of data
		
		// Send the actual number of nodes and the other static information contained in cmd
		return MaCaco_send(addr, MaCaco_DBSTRUCTANS, rx->putin, 0x00, rx->numberof, cmd);
	}
	
	#if(MaCaco_USERMODE && VNET_MEDIA1_ENABLE)	
	// answer to a discover request
	if (rx->funcode == MaCaco_DISCOVERREQ)
	{		
		// in the answer we include the IP address
		eth_GetIP(ipaddrs);
	
		// we just need to reply back, like a ping but available only on gateway nodes
		return MaCaco_send(addr, MaCaco_DISCOVERANS, rx->putin, 0x00, 0x04, ipaddrs);	
	}	
	#endif
	
	#if(DYNAMICADDRESSING && (MaCaco_USERMODE || VNET_SUPERNODE))	
	// answer to a dimanyc addressing request
	if (rx->funcode == MaCaco_DINADDRESSREQ)
	{	
		// Process new and yet issued requests
		if ((randomkeyid == 0) || (randomkeyid == (U16)rx->putin))
		{
			// If there are no pending request or a yet open pending request
			// proceed and provide an address
			
			// the putin is used as random key identifier
			randomkeyid = (U16)rx->putin;
			
			// the startoffset is used as media number
			// 
			// startoffset			media
			//		1				  1
			//		2				  2
			//		3				  3
			//		4				  4
			//		5				  5
			//	   F1				  1 as Supernode
			//	   F2				  2 as Supernode
			//	   F3				  3 as Supernode
			//	   F4				  4 as Supernode
			//	   F5				  5 as Supernode
			//
			U8 isSupernode = (rx->startoffset & 0xF0);
			U8 vNetMedia   = (rx->startoffset & 0x0F);
			
			U8 nodes=0;
			U16 nodeaddress=0x0002;
			
			// a supernode needs an address into another subnet
			if((isSupernode) && ((vNetMedia-1) != VNET_MEDIA1_ID))
			{
				// assign an address
				nodeaddress = vnet_addr_l[vNetMedia-1] + 1;
				
				// verify if there are other nodes in that subnet
				U8 tryagain=0, trying=0;
				do
				{
					// verify the address against all nodes
					for(nodes=0;nodes<MaCaco_NODES;nodes++)
					{
						// if there is yet another node in the subnet, move to the next one
						if((nodeaddress & DYNAMICADDR_SUBNETMASK) == ((*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*nodes)) && DYNAMICADDR_SUBNETMASK))
						{
							nodeaddress+=((~DYNAMICADDR_SUBNETMASK)+1);	//move to the next subnet					
							tryagain=1;
							trying++;
						}
					}	
				} 
				while(tryagain && (trying < MaCaco_NODES));		
						
				// validate address		
				if(!(vNetMedia == vNet_GetMedia(nodeaddress)))		
					return MaCaco_FUNCODE_ERR;
					
				// store the proposed address, will be used to identify later if the request
				// is completed
				proposedaddress = nodeaddress;
				
				// send the assigned address
				U8*	nodeaddress_p = (U8 *)(&nodeaddress);
				return MaCaco_send(0xFFFF, MaCaco_DINADDRESSANS, rx->putin, vNetMedia, 0x02, nodeaddress_p);
			}
			else	// is a standard node, gives an address in our subnet
			{
				// verify if there are yet devices on the same media
				for(nodes=0;nodes<MaCaco_NODES;nodes++)
					if(vNetMedia == vNet_GetMedia(*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*nodes)))
						nodeaddress++;
				
				// define the node address
				nodeaddress = nodeaddress + (vNet_GetAddress(vNetMedia) & vNet_GetSubnetMask(vNetMedia));
				
				// verify that the address isn't yet in use (as fixed address)
				U8 tryagain=0, trying=0;
				do
				{
					for(nodes=0;nodes<MaCaco_NODES;nodes++)
					{
						if(nodeaddress == (*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*nodes)))
						{
							nodeaddress++;						
							tryagain=1;
							trying++;
						}
					}	
				} 
				while(tryagain && (trying < MaCaco_NODES));
				
				// store the proposed address, will be used to identify later if the request
				// is completed
				proposedaddress = nodeaddress;
				
				// send the assigned address
				U8*	nodeaddress_p = (U8 *)(&nodeaddress);
				return MaCaco_send(0xFFFF, MaCaco_DINADDRESSANS, rx->putin, vNetMedia, 0x02, nodeaddress_p);
			}	
		}
		else
			return MaCaco_FUNCODE_ERR;		
	}		
	#endif
	
	/*********** Answer ************/
	
	// collect data from answer
	if (((rx->funcode & 0xF0) == 0x10) || ((rx->funcode & 0xF0) == 0x30) || ((rx->funcode & 0xF0) == 0x50))
	{
		#if(MaCaco_USERMODE)	
		// record a join request
		if (rx->funcode == MaCaco_JOINNETWORK)
		{			
			// look for a non used address register
			U8 nodes=0;
			while((((*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*nodes)) != addr) && ((*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*nodes)) != 0x0000)) && (nodes < MaCaco_NODES))
				nodes++;

			// if the node wasn't recorded, assign it
			if((*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*nodes) != addr) && nodes < MaCaco_NODES)
				(*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*nodes)) = addr;
	
			// sort the node addresses	
			U8 sort_i = 1, sorting = 0;
			U16	sort_buffer;
			U16* m_address = (U16 *)(memory_map + MaCaco_ADDRESSES_s);
	
			// out of this for all address are sorted, out of the local address
			for(sort_i=1; sort_i<MaCaco_NODES; sort_i++)
			{
				// don't sort zeros
				if(m_address[sort_i] == 0x0000) break;
				
				for(sorting=sort_i+1; sorting<MaCaco_NODES; sorting++)
				{
					// don't sort zeros
					if(m_address[sorting] == 0x0000) break; 
				
					// sort ascending
					if(m_address[sort_i] > m_address[sorting])
					{
						sort_buffer         = m_address[sort_i];
						m_address[sort_i]   = m_address[sorting];
						m_address[sorting]  = sort_buffer;
					}
				}
			}
			
			#if(MaCaco_DEBUG)
			MaCaco_LOG("(MaCaco)<ADDRS><");
			for(nodes=0; nodes<MaCaco_NODES; nodes++)
			{
				MaCaco_LOG("|0x");
				MaCaco_LOG((*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*nodes)),HEX);
				
			}
			MaCaco_LOG(">\r\n");
			#endif
			// restart the subscriptions
			for(U8 i=0;i<MaCaco_OUTMAXSUBSCR;i++)
				subscr_count[i] = 0;			
	
			// if the join request is from a nodes that previously got an address, flag the
			// request as completed
			if (randomkeyid == (U16)rx->putin)	// identify the node from the keyval
				randomkeyid = 0;
			else
			{
				// identify the node from the address
				for(nodes=0; nodes<MaCaco_NODES; nodes++)
					if(proposedaddress == (*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*nodes)))
						randomkeyid = 0;
			}		
	
			return MaCaco_FUNCODE_OK;	
		}	
		#endif
	
		#if(DYNAMICADDRESSING && VNET_MEDIA1_ENABLE)	
		// set an IP address at runtime
		if (rx->funcode == MaCaco_SETIP)
		{	
			// the payload contains the IPv4 address in the first four bytes
			// then the subnetmask and gateway IP
			U8 i, setbaseipaddr[4];
			U16 vnetaddress = 0;
			
			for(i=0;i<4;i++)
				setbaseipaddr[i] = (*(rx->data+i) & *(rx->data+4+i));					// Byte-wise AND to get the subnet IP address
			
			eth_SetBaseIP((uint8_t *)setbaseipaddr);									// Base IP address			
			eth_SetSubnetMask((uint8_t *)(rx->data+4));									// Subnetmask
			eth_SetGateway((uint8_t *)(rx->data+8));									// Gateway IP
			
			// use the last byte from the IP address to define the vNet one
			vnetaddress += (setbaseipaddr[3] & DYNAMICADDR_SUBNETMASK);
			
			vNet_SetAddress(vnetaddress, vNet_GetMedia(vnetaddress));																// Set vNet Address
			vNet_SetSubnetMask(DYNAMICADDR_SUBNETMASK, vNet_GetMedia(vnetaddress));													// Set vNet Subnetmask
			vNet_SetMySuperNode(((vnetaddress & DYNAMICADDR_SUBNETMASK) | DYNAMICADDR_GATEWAY), vNet_GetMedia(vnetaddress));		// Set vNet Supernode
		}
		#endif
		
		#if(DYNAMICADDRESSING)	
		// record the dynamic address provided
		if (rx->funcode == MaCaco_DINADDRESSANS)
		{			
			if(rx->numberof + 3 < MaCaco_CONFPARAM) // if the number of payload bytes, plus the size of rx->putin and rx->startoffset
			{
				// load data into the configuration parameters, an upper method will use them
				// for configuration purpose
				U8*	confparameters_p = (memory_map + MaCaco_CONFPARAM);
				
				// store the putin value
				(*(U16 *)confparameters_p) = (U16)rx->putin;
				confparameters_p += sizeof(U16);
				
				// store the startoffset value
				*confparameters_p = rx->startoffset;
				confparameters_p++;
				
				// load payload data
				if(rx->numberof)
					memmove(confparameters_p, rx->data, rx->numberof);
			}
		}
		#endif
		
		// Return for not expected functional codes
		if((rx->funcode == MaCaco_STATEANS))	// This functional codes is used only by user interfaces
			return MaCaco_FUNCODE_ERR;
	
		// check for operation type to separate bit-wise and not bit-wise operation
		switch(rx->funcode) 
		{
		// force register operation
		case(MaCaco_FORCEREGSTR) :
		
			memory_map += rx->startoffset;
			memmove(memory_map, rx->data, rx->numberof);
			return MaCaco_FUNCODE_OK;
		
		break;
		
		// bit-wise AND operation
		case(MaCaco_FORCEREGAND) :
			// no more than one register can be addressed with this functional code
			if (rx->numberof > 1)
				return MaCaco_ERR84;
			else if (rx->startoffset >= MaCaco_WRITE_s && rx->startoffset <= MaCaco_WRITE_f)
				{
					memory_map += rx->startoffset;	
					*memory_map &= *rx->data;	//bit-wise AND
					return MaCaco_FUNCODE_OK;	
				}	
		break;
		
		// bit-wise OR operation
		case(MaCaco_FORCEREGOR) :
			// no more than one register can be addressed with this functional code
			if (rx->numberof > 1)			
				return MaCaco_ERR84;
			else if (rx->startoffset >= MaCaco_WRITE_s && rx->startoffset <= MaCaco_WRITE_f)
				{
					memory_map += rx->startoffset;	
					*memory_map |= *rx->data;	//bit-wise OR
					return MaCaco_FUNCODE_OK;
				}
		break;

		// force by id
		#if(MaCaco_SUBSCRIBERS)
		case(MaCaco_FORCE) :
		
			// Offset is calculated while processing the request, the startoffset is the node address
			// in the memorymap
			
			if(rx->startoffset == MaCaco_LOCNODE)
			{
				memory_map += MaCaco_IN_s;
				memmove(memory_map, rx->data, rx->numberof);
				return MaCaco_FUNCODE_OK;
			}
			else	// Data shall be sent to a remote node
				return MaCaco_send(*(U16 *)(memory_map+MaCaco_ADDRESSES_s+rx->startoffset*2), MaCaco_FORCEREGSTR, 0, MaCaco_IN_s, rx->numberof, rx->data);
				
		break;
		#endif
	
		// force by typical logic number
		case(MaCaco_TYP) :
	
			// identify if the command is issued for a typical or a typical class
			if((rx->startoffset & 0x0F) == 0x00)
				typ_mask = 0xF0;	// we look only to the typical class value
			else
				typ_mask = 0xFF;	// we look to whole typical value

			// force typical on the local node
			for(U8 i=0; i<(MaCaco_TYPLENGHT - rx->numberof + 1); i++)		
				if((*(memory_map + MaCaco_TYP_s + i) & typ_mask) == rx->startoffset)	// Start offset used as typical logic indication
					for(U8 j=0; j < rx->numberof; j++)
						*(memory_map+MaCaco_IN_s + i + j) = *(rx->data + j);

			// force typical on the remote node
			#if(MaCaco_USERMODE)
			for(U8 j=0; j<(MaCaco_NODES-1-1); j++) 
			{
				// Send the force
				if(*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*(j+1)))											// The space used for data coming from other nodes has index 0 for node 1
					MaCaco_send(*(U16 *)(memory_map + MaCaco_ADDRESSES_s + 2*(j+1)), MaCaco_TYP, 0, rx->startoffset, rx->numberof, rx->data);				
			}
			#endif
									
			return MaCaco_FUNCODE_OK;
			
		break;
		
		// Typical logic answer
		#if(MaCaco_SUBSCRIBERS)
		case(MaCaco_TYPANS) :
			// This information is redirected to the User Interface and stored
			// only if data persistance is activated
				
			// Identify the node index
			U8 nodeindex;
			for(nodeindex=MaCaco_LOCNODE+1; nodeindex<MaCaco_NODES ; nodeindex++)
				if(addr == (*(U16*)(memory_map+MaCaco_ADDRESSES_s+2*nodeindex)))
					break;
			
			// Data persistance is active, store information
			#if(MaCaco_PERSISTANCE)
			if(rx->numberof)
				memmove((memory_map+MaCaco_P_TYP_s+(nodeindex*MaCaco_TYPLENGHT)), rx->data, rx->numberof);
			#elif(MaCaco_LASTIN)		// LASTIN is active, store the last information
			
			// Identify the first and last slot in the typical 5n group (analogue values)
			// this is an unconventional check at this layer, typicals are handled at a top
			// level
			i=0;
			while(((*(rx->data+i) & 0xF0) != 0x50) && i < MaCaco_SLOT)	i++;
			*(memory_map+MaCaco_L_TYP5n_s+2*nodeindex) = i; 
					
			while((((*(rx->data+i) == 0xFF) && ((*(rx->data+i) & 0xF0) == 0x50))  && i < MaCaco_SLOT))	i++;
			*(memory_map+MaCaco_L_TYP5n_s+2*nodeindex+1) = i;
			#endif
			
			if(reqtyp_addr)		// If there is a stored address from a User Interface
				MaCaco_send(reqtyp_addr, MaCaco_TYPANS, reqtyp_putin, nodeindex, rx->numberof, rx->data);								

		break;
		#endif
		
		// Subscription answer
		case(MaCaco_SUBSCRANS) :
		
			// Look for the subcription index
			i = 0;
			while ((subscr_outaddr[i] != addr) && i < MaCaco_OUTMAXSUBSCR)
				i++;
			
			// If no entry was found
			if(i == MaCaco_OUTMAXSUBSCR)
				return MaCaco_FUNCODE_ERR;
			
			// Set the subcription status flag
			if(subscr_outaddr[i] == addr)
				subscr_status[i] = 1;
			
			#if(MaCaco_SUBSCRIBERS)
			// This flag data subscribed by us as passtrough, shall be redirected to the User Interface
			if (rx->putin == 0)		
			{
				// Identify the node index
				U8 nodeindex;
				for(nodeindex=1; nodeindex<MaCaco_NODES ; nodeindex++)
					if(addr == (*(U16*)(memory_map+MaCaco_ADDRESSES_s+2*nodeindex)))
						break;

				#if(MaCaco_PERSISTANCE)		// PERSISTANCE is active, store information
				if(rx->numberof)
					memmove((memory_map+MaCaco_P_OUT_s+(nodeindex*MaCaco_OUTLENGHT)), rx->data, rx->numberof);
				#elif(MaCaco_LASTIN)		// LASTIN is active, store the last information
				// Identify a free space into the LASTIN data area
				i=0;
				while((*(memory_map+MaCaco_L_IDX_s+i) != MaCaco_L_IDX_NULL) && (i < MaCaco_L_BUFSIZE))	i++;
								
				// Store the node index
				*(memory_map+MaCaco_L_IDX_s+i) = nodeindex;
				
				// Store data
				if((i != MaCaco_L_BUFSIZE) && (rx->numberof))
					memmove((memory_map+MaCaco_L_OUT_s+(i*MaCaco_SLOT)), rx->data, rx->numberof);
				#endif
						
				// Send the data to all subscribers
				MaCaco_PassThrough_subAnswer(nodeindex, rx->numberof, rx->data);
				
				return MaCaco_FUNCODE_OK;
			}
			else	// This flag data subscribed for local use
			#endif
			{
				if ((rx->putin >= &memory_map[MaCaco_WRITE_s] && rx->putin <= &memory_map[MaCaco_WRITE_f]) && (rx->numberof > 0))				
					memmove(rx->putin, rx->data, rx->numberof); // data collected in putin address
				else
					return MaCaco_FUNCODE_ERR;				
			}			
				
			return MaCaco_FUNCODE_OK;
			
		break;
		
		// All others functional codes
		default :
			if ((rx->putin >= &memory_map[MaCaco_WRITE_s] && rx->putin <= &memory_map[MaCaco_WRITE_f]) && (rx->numberof > 0))				
				memmove(rx->putin, rx->data, rx->numberof); // data collected in putin address			
			return MaCaco_FUNCODE_OK;
			
		break;
		}	
	}	
}

/**************************************************************************/
/*!
	Look for incoming data from all the available vNet media	
 */
/**************************************************************************/
void MaCaco_DataIn()
{
	// If there are no pending incoming data, look for new incoming data
	if(!MaCaco_rx.datain)
		MaCaco_rx.datain = vNet_DataAvailable();

	else
	{	
		MaCaco_rx.datain--;		// Update the counter
		if(!MaCaco_rx.datain) MaCaco_rx.datain = vNet_DataAvailable();
	}
}
/**************************************************************************/
/*!
    Read data from the network and call protocol functions, change this 
	function to use a different hardware protocol.
	
	Multiple memory maps management is not allowed.
*/
/**************************************************************************/
U8 MaCaco_retrieve(U8* memory_map, U8* data_chg)
{
	U8 used_media=0, len, status=0;
	U16 src_addr = 0x0000;

	// If data are available
	if(MaCaco_rx.datain > 0)
	{
		// get data from data buffer
		len = vNet_RetrieveData(&MaCaco_data[0]);		
		
		// parse the received message
		if ((len != VNET_DATA_FAIL) && (len != VNET_DATA_ROUTED))
		{				
			// get source address
			used_media = vNet_GetLastMedia();
			src_addr = vNet_GetoSourceAddress(used_media);

			MaCaco_rx.data = &MaCaco_data[0];
			status = MaCaco_parse(&MaCaco_rx);
			
			// analyze data and take action as per protocol
			if (status != MaCaco_FUNCODE_ERR)
			{
					MaCaco_peruse(src_addr, &MaCaco_rx, memory_map);
					
					return MaCaco_rx.funcode;
			}
			else
				status = MaCaco_FUNCODE_ERR;
		}
		else
			status = MaCaco_NODATARECEIVED;
	}

	// if there was a change in the memory map
	if (*data_chg == MaCaco_DATACHANGED)
	{

		// send data to all subscriptors
		status = MaCaco_subAnswer(memory_map, data_chg);	
		
		#if(MaCaco_PERSISTANCE)			// PERSISTANCE is active, store data
			memmove((memory_map+MaCaco_P_OUT_s), (memory_map+MaCaco_OUT_s), MaCaco_SLOT);
		#elif(MaCaco_LASTIN)			// LOCALIN is active, store data
			// Identify a free space into the LASTIN data area
			U8 i=0;
			while((*(memory_map+MaCaco_L_IDX_s+i) != MaCaco_L_IDX_NULL) && (i < MaCaco_L_BUFSIZE))	i++;

			if(i != MaCaco_L_BUFSIZE)
			{		
				// Store the node index
				*(memory_map+MaCaco_L_IDX_s+i) = 0;											// Local node has index 0
								
				// Store the data
				memmove((memory_map+MaCaco_L_OUT_s+(i*MaCaco_SLOT)), (memory_map+MaCaco_OUT_s), MaCaco_SLOT);
			}
			
		#endif
	}	
		
	return status;	
}

/**************************************************************************/
/*!
    Send data to all subscribers nodes
*/
/**************************************************************************/
U8 MaCaco_PassThrough_subAnswer(U8 startoffset, U8 numberof, U8 *data)
{
	U8 status=0, i=0, j=0;

	// Send data to all subscribers
	while (i < MaCaco_INMAXSUBSCR)
	{
		if(subscr_addr[i]!=0x0000)
		{		
			// Send data to subscribers
			status = MaCaco_send(subscr_addr[i], MaCaco_STATEANS, subscr_putin[i], startoffset, numberof, data);
			
			// If data are successfully sent, go to next subscription					
			if(status)
			{
				i++;
				j=0;
			}	
			else
				j++;
		}
		else
		{
			i++;
			j=0;
		}	

		// Retry no more that MAXRETRY number
		if(j>Macaco_MAXRETRY)
		{			
			// Next subscription
			i++;
			j=0;
		}
	}
	
	// Return the completition status
	return status;
}

/**************************************************************************/
/*!
    Send data to all subscribers nodes, when working in UserMode
*/
/**************************************************************************/
U8 MaCaco_subAnswer(U8* memory_map, U8* data_chg)
{
	U8 status=0, i=0, j=0;

	// Send data to all subscribers
	while (i < MaCaco_INMAXSUBSCR)
	{
		if(subscr_addr[i]!=0x0000)
		{
			// answer to subscription
			if(subscr_funcode[i] == MaCaco_SUBSCRREQ)
				status = MaCaco_send(subscr_addr[i], MaCaco_SUBSCRANS, subscr_putin[i], subscr_startoffset[i], subscr_numberof[i], (subscr_startoffset[i] + memory_map));				
			else if(subscr_funcode[i] == MaCaco_STATEREQ)
			{			
				U16 nodeoffest, len;
				
				// These points the local data
				nodeoffest = MaCaco_OUT_s;
				len = MaCaco_OUTLENGHT;							

				// The nodes contains only local data, send them
				status = MaCaco_send(subscr_addr[i], MaCaco_STATEANS, subscr_putin[i], subscr_startoffset[i], len, (nodeoffest + memory_map));
			}		
		
			// If data are successfully sent, go to next subscription					
			if(status)
			{
				i++;
				j=0;
			}	
			else
				j++;
		}
		else
		{
			i++;
			j=0;
		}	

		// Retry no more that MAXRETRY number
		if(j>Macaco_MAXRETRY)
		{			
			// Next subscription
			i++;
			j=0;
		}
	}
		
	if(data_chg) *data_chg = MaCaco_NODATACHANGED;
	status = MaCaco_NODATARECEIVED;
	
	return status;
}

/**************************************************************************/
/*!
    Count for how many nodes typicals values was requested, this is set
	when the User Interface request the typicals values and count for
	the number of nodes to be requested.
*/
/**************************************************************************/
U8 MaCaco_reqtyp()
{	
	if(reqtyp_times)
		return reqtyp_times--;
}


/**************************************************************************/
/*!
    Activate a subscription on remote device, send periodically subscription
	request based on communication healthy. At first run, healthy value should
	be greater than MaCaco_SUBSCRHEALTY.
	
	For each subscripted device, an unique subscribe_addr (starting from 0x00)
	should be used, subscribed data will be stored in the putin address.
	
	Multiple subscribed memory maps management is allowed for subscribed data, 
	the subscribed memory maps are not shared with other network devices and
	can be (or not) different from the shared memory map.
*/
/**************************************************************************/
#if(MaCaco_SUBSCRIBERS)
U8 MaCaco_subscribe(U16 addr, U8 *memory_map, U8 *putin, U8 startoffset, U8 numberof, U8 subscr_chnl)
{
	U8 *healty, *count = 0;

	// Verify the subscription index
	if(subscr_chnl >= MaCaco_OUTMAXSUBSCR)
		return MaCaco_NOSUBSCRANSWER;
	
	// Init the pointers
	healty = memory_map+MaCaco_HEALTY_s+subscr_chnl;
	count = subscr_count+subscr_chnl;
	
	// Record the output subscription
	subscr_outaddr[subscr_chnl] = addr;
	
	// If the subscribed node is battery operated we cannot perform an healthy
	// check, because than node will spent its time sleeping. 
	if(subscr_battery[subscr_chnl])
		*healty = MaCaco_SUBMAXHEALTY;
	
	// If an answer was recorded, increase healthy value
	if (subscr_status[subscr_chnl] == 1)
	{
		if (*healty < MaCaco_SUBSCRHEALTY)
			*healty = 2*MaCaco_SUBSCRHEALTY;
		else
		{
			if (*healty < (MaCaco_SUBMAXHEALTY-MaCaco_SUBSINCREASE)) 
				(*healty)+=MaCaco_SUBSINCREASE;
			else
				(*healty)=MaCaco_SUBMAXHEALTY;
		}
		
		// Update the delay counter
		*count = *healty;
		
		// Reset subscription status flag
		subscr_status[subscr_chnl] = 0;
	}	
	else
	{
		// if delay is expired subscript again and decrease healthy
		if ((*count < MaCaco_SUBSCRHEALTY))
			{
				if ((*healty > 0x00) && (MaCaco_send(addr, MaCaco_SUBSCRREQ, putin, startoffset, numberof, 0x00))) 
					(*healty)-=MaCaco_SUBSDECREASE;
								
				*count = MaCaco_SUBMAXHEALTY;	// Delay the next try
			}
		else
			if (*count > 0x00) (*count)--;
	}		

	// return if channel is healty or failed
	if (*healty < MaCaco_SUBSCRHEALTY) 		
		return MaCaco_NOSUBSCRANSWER;
	else
		return MaCaco_SUBSCRANSWER;
		
}


/**************************************************************************/
/*!
    Reset the subscription, this will force all subscripted channel to 
	ask for fresh data
*/
/**************************************************************************/
void MaCaco_subscribe_reset()
{
		for(U8 i=0;i<MaCaco_OUTMAXSUBSCR;i++)
			subscr_count[i] = 0;
}

/**************************************************************************/
/*!
    Flag that the subscribed node is battery operated
*/
/**************************************************************************/
void MaCaco_subscribe_battery(U8 subscr_chnl)
{
	subscr_battery[subscr_chnl] = true;
}
#endif

/**************************************************************************/
/*!
    Record a subscription as it was received from a node, this methods
	is generally not required and simulate into the node a subscription
	request.
	
	It can be used to hard code a subscription in nodes that are not powered
	24h and that can never match a subscription request (because the request
	is sent when the node is powered off).
	
	If the data from both the side of the subscription (subscriber node and
	subscripted node) are not matching, data are not trasferred or discarded.
*/
/**************************************************************************/
void MaCaco_subscribe_record(U16 addr, U8 funcode, U16 putin, U8 startoffset, U8 numberof)
{
	// look for the subscription channel
	U8 i = 0;
	while((subscr_addr[i] != addr) && (subscr_addr[i] != 0x0000) && (i < MaCaco_INMAXSUBSCR))
		i++;
			
	// Store the subscription data
	if(i < MaCaco_INMAXSUBSCR)
	{
		subscr_addr[i]  = addr;
		subscr_funcode[i] = funcode;
		subscr_putin[i] = (U8*)putin;
		subscr_startoffset[i] = startoffset;
		subscr_numberof[i] = numberof;
	}
}

/**************************************************************************/
/*!
    Retrieve the latest functional code
*/
/**************************************************************************/
U8 MaCaco_getfuncode()
{
	return MaCaco_rx.funcode;
}

/**************************************************************************/
/*!
    Retrieve the latest putin address
*/
/**************************************************************************/
U16 MaCaco_getputin()
{
	return (U16)MaCaco_rx.putin;
}

/**************************************************************************/
/*!
    Retrieve the latest startoffset value
*/
/**************************************************************************/
U8 MaCaco_getstartoffset()
{
	return MaCaco_rx.startoffset;
}

/**************************************************************************/
/*!
    Retrieve the latest numberof value
*/
/**************************************************************************/
U8 MaCaco_getnumberof()
{
	return MaCaco_rx.numberof;
}

/**************************************************************************/
/*!
    Retrieve the latest datain value
*/
/**************************************************************************/
U8 MaCaco_getdatain()
{
	return MaCaco_rx.datain;
}


/**************************************************************************/
/*!
    Get the buffer pointer
*/
/**************************************************************************/
U16 MaCaco_getdatabuffer()
{
	return (U16)(&MaCaco_data[0]);
}

/**************************************************************************/
/*!
    Return the subscription state, if true a gateway node has subscribed 
	this node
*/
/**************************************************************************/
U8 MaCaco_IsSubscribed()
{
	U8 i=0;
	
	// stops at first subcription
	while((subscr_addr[i]==0) && (i<(MaCaco_INMAXSUBSCR-1)))
		i++;
	
	// returns subscription state
	return (subscr_addr[i]!=0);	
	
}

/**************************************************************************/
/*!
    Record an internal subscription
		
		Using this method the gateway starts to subscribe all the peers
		if the PERSISTANCE or LASTIN mode is active, those data are stored in 
		the gateway and became available for external protocols.
		
		If any full compliant MaCaco User Mode Interface is used, this
		method isn't longer required, because the subscription for the user
		interface is also used for external protocols.
*/
/**************************************************************************/
void MaCaco_InternalSubcription(U8 *memory_map)
{
	U8 i=0;
	
	/** Init the typicals **/

	#if(MaCaco_PERSISTANCE)			// PERSISTANCE is active, store data
		memmove((memory_map+MaCaco_P_TYP_s), (memory_map+MaCaco_TYP_s), MaCaco_SLOT);		
	#elif(MaCaco_LASTIN)			// LOCALIN is active, store data	
		// Clear the index list
		for(i=0;i<MaCaco_L_BUFSIZE;i++)
			*(memory_map+MaCaco_L_IDX_s+i) = MaCaco_L_IDX_NULL;
		
		// Clear the typical list
		for(i=0;i<(2*MaCaco_NODES);i++)
			*(memory_map+MaCaco_L_TYP5n_s+i) = MaCaco_SLOT;			// This is the index of the first and last
																	// analog T5n type typical logic, we set it
																	// at last available value.
		
		// Indentify the first and last slot in the typical 5n group (analogue values) 
		// this an unconventional check at this layer
		i=0;
		while(((*(memory_map+MaCaco_TYP_s+i) & 0xF0) != 0x50) && i < MaCaco_SLOT)	i++;
		*(memory_map+MaCaco_L_TYP5n_s) = i; 
				
		while(((*(memory_map+MaCaco_TYP_s+i) == 0xFF) || ((*(memory_map+MaCaco_TYP_s+i) & 0xF0) == 0x50))  && i < MaCaco_SLOT)	i++;
		*(memory_map+MaCaco_L_TYP5n_s+1) = i;
		
	#endif
							
	/** Create a permanent data subscription **/

	// look for the subscription channel
	i = 0;
	while((subscr_addr[i] != VNET_ADDR_NULL) && (subscr_addr[i] != 0x0000) && (i < MaCaco_INMAXSUBSCR))
		i++;
			
	// Store the subscription data
	if(i < MaCaco_INMAXSUBSCR)
	{
		subscr_addr[i]  = VNET_ADDR_NULL;
		subscr_funcode[i] = MaCaco_STATEREQ;
		subscr_putin[i] = 0x00;
		subscr_startoffset[i] = 0x00;		
		subscr_numberof[i] = MaCaco_NODES;
	}
	
	/** Create a one time request for typicals logics values **/

	// Record the request info
	reqtyp_addr = VNET_ADDR_NULL;	
	reqtyp_putin = 0x00;
	reqtyp_startoffset = 0x00;
	reqtyp_numberof = MaCaco_SLOT;
				
	// Flag that the request shall be processed for all nodes, this is used at an upper level
	reqtyp_times = MaCaco_NODES;		
			
}

/**************************************************************************/
/*!
    Return if are LASTIN data available
*/
/**************************************************************************/
U8 MaCaco_isLastIn(U8 *memory_map)
{
	if(MaCaco_GetLastIn(memory_map) == MaCaco_L_IDX_NULL)
		return MaCaco_FUNCODE_ERR;
		
	return MaCaco_FUNCODE_OK;
}

/**************************************************************************/
/*!
    Return the node number of the first available value into the LASTIN 
	data area
*/
/**************************************************************************/
U8 MaCaco_GetLastIn(U8 *memory_map)
{
	// Get the index
	U8 i=0;
	while((*(memory_map+MaCaco_L_IDX_s+i) == MaCaco_L_IDX_NULL) && (i < MaCaco_L_BUFSIZE))	i++;
	
	if(i == MaCaco_L_BUFSIZE)
		return MaCaco_L_IDX_NULL;										// No new data
	
	return *(memory_map+MaCaco_L_IDX_s+i);
}

/**************************************************************************/
/*!
    Return the index of the first available value into the LASTIN 
	data area
*/
/**************************************************************************/
U8 MaCaco_GetLastIndex(U8 *memory_map, U8 nodenumber)
{
	// Get the index
	U8 i=0;
	
	if(nodenumber == MaCaco_L_IDX_NULL)
		return MaCaco_L_IDX_NULL;
	
	while((*(memory_map+MaCaco_L_IDX_s+i) != nodenumber) && (i < MaCaco_L_BUFSIZE))	i++;
	
	if(i == MaCaco_L_BUFSIZE)
		return MaCaco_L_IDX_NULL;										// No new data
	
	*(memory_map+MaCaco_L_IDX_s+i) = MaCaco_L_IDX_NULL;
	
	// Return the index
	return i;
}