/*
 * twip.cpp - TWI Protocol library
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

#include <Arduino.h>
#include "utility/cb.h"
#include "twip.h"

extern "C" {
	#include "utility/twi.h"
};

/*
 * Function: class constructor
 *    Input: uint8_t addr is the TWI address that this master will use.
 *   Output: No output.
 *
 * Description: The principle behind this library requires that every participating uC to be
 * addressed on the TWI bus thus allowing it to change from master transmitter to receiver.
 *
 */
twiprotocol::twiprotocol( uint8_t addr ) {
	this->twi_address = addr;
	this->rx_buffer.init( TWIP_MAX_BUFFER_SIZE );

	twi_attachSlaveRxEvent( twip_onreceive );
	twi_setAddress( addr );
	twi_init();
}

/*
 * Function: twiprotocol::checksum
 *    Input: uint8_t sender, flag, opcode, id, len are extracted from packet's header
 *   Output: uint16_t (2 bytes) checksum value.
 *
 * Description: Packet's sender and flag are packet together to minimize uint16_t overflow because
 * sender's address range will be between 0 and 127 and flag have a limited value.
 *
 */
uint16_t twiprotocol::checksum( uint8_t sender, uint8_t flag, uint8_t opcode, uint8_t id, uint8_t len ) {
	return ~( ((sender << 8) + opcode) + (((flag + len) << 8) + id ));
}

/*
 * Function: twiprotocol::flag_decode
 *    Input: uint8_t type can be TWIP_FLAG_NFO for the fragment information TWIP_FLAG_TTL for the TTL value,
 *           int8_t flag is raw flag byte fetched from a packet.
 *   Output: One byte (uint8_t) shifted to the right.
 *
 * Description: Returns the user selected part of the packet's flag byte. Please read twiprotocol::receive()
 * for THE complete description about packet's flag.
 *
 */
uint8_t twiprotocol::flag_decode( uint8_t type, uint8_t flag ) {
	uint8_t ret = flag;
	switch( type ) {
		case TWIP_FLAG_NFO: ret <<= 6; ret >>= 6; break;
		case TWIP_FLAG_TTL: ret >>= 4; break;
	}
	return ret;
}

/*
 * Function: twiprotocol::rx_add
 *    Input: uint8_t* data is the packet's payload to be added to the rx buffer,
 *           int bytes is the total size of packet's payload.
 *   Output: uint8_t (bool) 1 - Success, 0 - Failure.
 *
 * Description: A valid twip packet must be at least 7 bytes long and with a valid header checksum. If the
 * packet clears the validation then it tries to reserve enough memory on the queue to store the data.
 *
 */
uint8_t twiprotocol::rx_add( uint8_t* data, int bytes ) {
	// A valid twip packet must be at least TWIP_HEADER_SIZE (aligned on a boundary of 4) bytes long,
	// packet's checksum must match header's checksum and enough available memory must exist on rx
	// buffer, if any of those conditions are not true ignore packet.
	if( bytes < ( TWIP_HEADER_SIZE + 3 ) & ~0x03 || (TWIP_HEADER_SIZE + data[6] +1) > this->rx_buffer.available() ||
		(uint16_t) ((data[4] << 8) + data[5]) != this->checksum(data[0], data[1], data[2], data[3], data[6]) ) { return false; }

	// Add the accounting byte
	this->rx_buffer.write( TWIP_HEADER_SIZE + data[6] );

	// Loop trough the payload and copy byte by byte to rx buffer
	for( uint8_t i = 0; i < (TWIP_HEADER_SIZE + data[6]); i++ ) { this->rx_buffer.write( data[i] ); }

	#ifdef __INFO2____
	Serial.print( "rx: " );
	Serial.print( bytes );
	Serial.print( ", cb: " );
	Serial.println( this->rx_buffer.available() );
	#endif

	return true;
}

/*
 * Function: twiprotocol::send
 *    Input: Packet's basic info (header) and payload.
 *   Output: Boolean representing: 1 - Success, 0 - Failure.
 *
 * Description: Fragments the packet if required and send it over the TWI bus, look up flag
 * meaning on the following table:
 *
 *	B00000000 - Packet not fragmented
 *	B00000001 - Fragmented packet / first fragmented packet of a set
 *	B00000011 - Last fragmented packet of a set
 *
 */
uint8_t twiprotocol::send( uint8_t addr, uint8_t opcode, uint8_t bytes, uint8_t* payload ) {
	// Finds out the number of twip packets required to send payload.
	// uint8_t packets is not declared as float on propose, uint8_t bytes excludes header size.
	uint8_t packets = (bytes / (TWI_BUFFER_LENGTH - TWIP_HEADER_SIZE)) +1;
	if( bytes % (TWI_BUFFER_LENGTH - TWIP_HEADER_SIZE) == 0 ) { packets--; }
	if( packets == 0 ) { packets++; } // For packets without payload
	uint8_t ret = false;

	uint8_t t_payload_cur = 0;

	for( uint8_t i = 0; i < packets; i++ ) {
		uint8_t t_this_pkt_len = bytes;
		if( bytes > (TWI_BUFFER_LENGTH - TWIP_HEADER_SIZE) ) { t_this_pkt_len = (TWI_BUFFER_LENGTH - TWIP_HEADER_SIZE); }
		uint8_t t_this_pkt_aligned = ( (TWIP_HEADER_SIZE + t_this_pkt_len) + 3 ) & ~0x03;

		// Allocate the required memory block for packet
		uint8_t* packet = (uint8_t *) malloc( sizeof(uint8_t) * t_this_pkt_aligned );

		// Populate packet's header with basic information
		packet[0] = this->twi_address;
		packet[1] = ( packets < 2 ) ? TWIP_NOF : TWIP_SOF;
		packet[2] = opcode;
		packet[3] = this->pkt_id;
		packet[6] = t_this_pkt_len;

		// Last packet change flag's 2nd bit to 1 (AVR architecture is little endian)
		if( (packets > 1 ) && (i == (packets -1)) ) { packet[1] = TWIP_EOF; }

		// Checksum is the last thing to be calculated
		packet[4] = this->checksum( packet[0], packet[1], packet[2], packet[3], packet[6] ) >> 8;
		packet[5] = this->checksum( packet[0], packet[1], packet[2], packet[3], packet[6] );

		// Copy the payload into packet
		for( uint8_t j = TWIP_HEADER_SIZE; j < t_this_pkt_len; j++ ) {
			packet[j] = payload[ t_payload_cur ];
			t_payload_cur++;
		}

		// NULL fill the packet aligned on boundary of four
		for( uint8_t j = (TWIP_HEADER_SIZE + t_this_pkt_len); j < t_this_pkt_aligned; j++ ) {
			packet[ TWIP_HEADER_SIZE + j ] = 0x00;
		}

		// Only increase packet id if no fragmentation is required
		if( packets < 2 ) { this->pkt_id++; }

		// Send the packet over the TWI bus and report return value
		ret = twi_writeTo( addr, packet, t_this_pkt_aligned, true, true );
		// TODO Take advantage of the new repeated start feature on the TWI library.
		//(packets == i +1) ? true : false

		#ifdef __INFO2____
		switch( ret ) {
			case 0: Serial.print( "tx: " ); Serial.println( t_this_pkt_aligned ); ret = true; break;
			case 1: Serial.println( "Length too long for buffer" ); break;
			case 2: Serial.println( "Address send, NACK received" ); break;
			case 3: Serial.println( "Data send, NACK received" ); break;
			default: Serial.println( "Other TWI error" ); break;
		}
		#endif

		#ifdef __INFO5____
		Serial.print( "tx: " );
		for( uint8_t j = 0; j < TWIP_HEADER_SIZE + packet[6]; j++ ) {
			Serial.print( "0x" );
			Serial.print( packet[j], HEX );
			Serial.print( " " );
		} Serial.println();
		#endif

		// Data accounting
		bytes -= t_this_pkt_len;

		// Cleanup
		free( packet );
	}

	// Now we can increase packet id for fragmented packets
	if( packets > 1 ) { this->pkt_id++; }

	return ret;
}

/*
 * Function: twiprotocol::receive
 *    Input: No input.
 *   Output: twippacket structure with header and payload information filled.
 *
 * Description: Fetches the first packet from rx_buffer returning a twipacket structure. The rx queue's
 * policy is FIFO meaning that ascending array index is descending age of packet, to put it on another
 * words the lower index of the rx buffer is always the oldest packet on buffer and it will always be
 * fetched first. It's also important to note the complete flag, that flag will indicate if we were
 * able to return all fragments of the same packet.
 *
 **** MORE INFORMATION ****
 * A few words about the packet's flag, to start take note that AVR is little endian (LSB).
 * The byte flag is split into three blocks, the first block are the bits 1 and 2, the second block
 * are bits 3 and 4 and the last block are bits 5, 6, 7 and 8. If the first bit is set (TWIP_SOF) then
 * the packet is fragmented, the second bit will always be unset for every fragment of the same packet
 * and it will be set (TWIP_EOF) for the last fragment. The third and fourth bits are currently unused
 * and are internally reserved. The remaining four bits represent the packet's TTL (time-to-live) with
 * a maximum binary value 0x0F, please note that TTL value will not be sequential when increasing (+1).
 */
twippacket twiprotocol::receive( void ) {
	twippacket ret;
	ret.complete = false;

	// Don't do anything if buffer is empty.
	if( this->rx_buffer.empty() ) { return ret; }

	uint8_t t_count = 0;
	uint8_t t_total_bytes = 0;

	// Allocate the initial memory block for payload
	ret.payload = (uint8_t *) malloc( sizeof(uint8_t) );

	// Loop until a complete packet is found or reach the end of the available bytes on buffer.
	while( ! ret.complete ) {
		uint8_t t_flag = this->flag_decode( TWIP_FLAG_NFO, this->rx_buffer.peek(2) );
		uint8_t t_size = this->rx_buffer.read() - TWIP_HEADER_SIZE;

		t_total_bytes += t_size;
		t_count++;

		if( t_count == 1 ) { // Fetch header only for the first packet
			ret.sender		= this->rx_buffer.read();
			ret.flag		= this->rx_buffer.read();
			ret.opcode		= this->rx_buffer.read();
			ret.id			= this->rx_buffer.read();
							  this->rx_buffer.read(); // Ignore checksum bytes
							  this->rx_buffer.read(); // same here
			ret.size		= this->rx_buffer.read();

		} else { for( uint8_t i = 0; i < TWIP_HEADER_SIZE; i++ ) {
				this->rx_buffer.read(); // Ignore everything
		} }

		// Reallocate the memory block to accommodate the additional payload data
		uint8_t* np = (uint8_t *) realloc( ret.payload, sizeof(uint8_t) * t_total_bytes );
		if( np != NULL ) { ret.payload = np; }

		// Actually copy the data from buffer to twippacket's payload
		for( uint8_t i = 0; i < t_size; i++ ) {
			ret.payload[ (t_total_bytes - t_size) + i ] = this->rx_buffer.read();
		}

		// Decides whether the packet is complete
		switch( t_flag ) {
			case TWIP_NOF: ret.complete = true;
				break;

			case TWIP_EOF:
				ret.flag = TWIP_EOF;
				ret.complete = true;
				break;
		}

		// If at the end of buffer break the cycle
		if( this->rx_buffer.empty() ) { break; }
	}

	// Update header with total bytes read and checksum
	ret.size = t_total_bytes;
	ret.checksum = this->checksum( ret.sender, ret.flag, ret.opcode, ret.id, ret.size );

	if( ! ret.complete ) { // No complete packet found
		// Checks if it's possible to restore the packet into buffer
		if( ret.size + TWIP_HEADER_SIZE < this->rx_buffer.available()
			&& this->flag_decode(TWIP_FLAG_TTL, ret.flag) < TWIP_MAX_TTL ) {
			uint8_t t_flag = this->flag_decode( TWIP_FLAG_TTL, ret.flag );
			t_flag++; // Increase packet's TTL

			this->rx_buffer.write( ret.size + TWIP_HEADER_SIZE );
			this->rx_buffer.write( ret.sender );
			this->rx_buffer.write( (t_flag << 4) + this->flag_decode(TWIP_FLAG_NFO, ret.flag) );
			this->rx_buffer.write( ret.opcode );
			this->rx_buffer.write( ret.id );
			this->rx_buffer.write( ret.checksum >> 8 );
			this->rx_buffer.write( ret.checksum );
			this->rx_buffer.write( ret.size );

			// Move the payload back to the buffer
			for( uint8_t i = 0; i < ret.size; i++ ) {
				this->rx_buffer.write( ret.payload[i] );
			}
		}

		// Deallocate the memory block
		uint8_t* np = (uint8_t *) realloc( ret.payload, sizeof(uint8_t) );
		if( np != NULL ) { ret.payload = np; }
	}

	return ret;
}

/*
 * Function: twiprotocol::available
 *    Input: No input.
 *   Output: Boolean representing: 1 - At least one packets is available, 0 - No packets available.
 *
 * Description: Wrapper function to return the total number of packets currently at rx_buffer
 *
 */
uint8_t twiprotocol::available( void ) { return ! this->rx_buffer.empty(); }

/*
 * Function: twiprotocol::put
 *    Input: data is a pointer to payload to be added to the rx_buffer,
 *           bytes is the total size of payload.
 *   Output: Boolean representing: 1 - Success, 0 - Failure.
 *
 * Description: Public method acting as a wrapper to rx_add(), defined because it is a more (I hope)
 * user friendly name; rx_add() is the private method, so no access to it outside the class scope.
 *
 */
uint8_t twiprotocol::put( uint8_t* data, int bytes ) { return this->rx_add( data, bytes ); }

/*
 * Function: twip_onreceive
 *    Input: uint8_t* data is the packet's payload to be added to the rx_buffer,
 *           int bytes is the total size of packet's payload.
 *   Output: No output.
 *
 * Description: Wrapper function called by TWI when TW_SR_STOP is received on the bus. This function
 * MUST be the less cycle intensive possible, meaning that no fancy stuff like Serial.print() nor delay()
 * nor any other crap that could block the TWI bus SHOULD be used here.
 *
 */
void twip_onreceive( uint8_t* data, int bytes ) { twip.put( data, bytes ); }