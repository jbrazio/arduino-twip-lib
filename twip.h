/*
 * twip.h - TWI Protocol library
 * Copyright (c) 2012 João Brázio <joao@brazio.org>, all rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __twip_h____
#define __twip_h____

#include <Arduino.h>
#include "utility/cb.h"

#define TWIP_MAX_TTL 0x0F
#define TWIP_HEADER_SIZE 7
#define TWIP_MAX_BUFFER_SIZE 254

#define TWIP_NOF 0x00	// No fragmentation
#define TWIP_SOF 0x01	// Start of fragmentation
#define TWIP_EOF 0x03	// End of fragmentation

#define TWIP_FLAG_NFO 0x00	// Packet's header fragmentation flag
#define TWIP_FLAG_TTL 0x01	// Packet's header TTL flag

struct twippacket {
	uint8_t  sender;
	uint8_t  flag;
	uint8_t  opcode;
	uint8_t  id;
	uint16_t checksum;
	uint8_t  size;
	uint8_t  complete;
	uint8_t* payload;
};

class twiprotocol {
	private:
		cb rx_buffer;
		uint8_t pkt_id;
		uint8_t twi_address;

		uint8_t		rx_add( uint8_t* data, int bytes );
		uint8_t		flag_decode( uint8_t type, uint8_t flag );
		uint16_t	checksum( uint8_t sender, uint8_t flag, uint8_t opcode, uint8_t id, uint8_t len );

	public:
		twiprotocol( uint8_t addr );

		twippacket	receive( void );
		uint8_t		available( void );
		uint8_t		put( uint8_t* data, int bytes );
		uint8_t		send( uint8_t addr, uint8_t opcode, uint8_t bytes = 0, uint8_t* payload = NULL );
};

extern twiprotocol twip;
void twip_onreceive( uint8_t* data, int bytes );

#endif