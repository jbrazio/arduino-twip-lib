/*
 * twip_led_blink.ino
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
 
/*
 * HOW TO USE THIS EXAMPLE
 *
 * Get two arduino boards and connect +5V, GND, A4 and A5 between them. Connect the first arduino
 * board to your computer and upload this sketch with TWI_ADDRESS defined as 1 and TWI_OTHER_ADDRESS
 * defined as 2. Then connect the other board and define the value of TWI_ADDRESS as 2 and the value
 * of TWI_OTHER_ADDRESS as 1. You can find the definition of TWI_ADDRESS at twip_led_blink.h line 23
 * and TWI_OTHER_ADDRESS at twip_led_blink.ino (this file) line 44.
 *
 * The LED on PIN 13 of both boards will start blinking randomly based on the received opcode:
 * 	- opcode 1 will light the LED
 *	- opcode 2 will turn off the LED
 *	- opcode 3 will reverse the current state of the LED
 *
 */

#include <Arduino.h>
#include <twip.h>
#include "twip_led_blink.h"

#ifdef TWI_ADDRESS // TWI Protocol init
twiprotocol twip = twiprotocol( TWI_ADDRESS );
#endif

#define TWI_OTHER_ADDRESS 2

void setup( void ) {
	unsigned long seed = 0, count = 32;
	while( count-- ) { seed = (seed<<1) | (analogRead(0)&1); }
	randomSeed( seed );

	Serial.begin( 19200 );
	Serial.println( "uC running" );
	
	if( LED_HEARTBEAT ) { pinMode( LED_HEARTBEAT, OUTPUT ); }
}

void loop( void ) {
	if( millis() - timer_main > 500 ) {
		// Reset the main timer counter
		timer_main = millis();
			
		uint8_t rnd = random( 7 );
		switch( rnd ) {
			case 1:
				twip.send( TWI_OTHER_ADDRESS, 1 ); // opcode 1 means LED ON
				break;
				
			case 3:
				twip.send( TWI_OTHER_ADDRESS, 2 ); // opcode 2 means LED OFF
				break;
				
			case 6:
				twip.send( TWI_OTHER_ADDRESS, 3 ); // opcode 2 means ! LED
				break;
			
			default: break;
		}

		while( twip.available() ) {
			twippacket pkt = twip.receive();

			if( pkt.complete ) {
				Serial.print( "sender: " );
				Serial.print( pkt.sender );

				Serial.print( ", opcode: " );
				Serial.print( pkt.opcode );

				Serial.print( ", size: " );
				Serial.println( pkt.size );
				
				switch( pkt.opcode ) {
					case 1:
						if( LED_HEARTBEAT ) { digitalWrite( LED_HEARTBEAT, HIGH ); }
						break;
						
					case 2:
						if( LED_HEARTBEAT ) { digitalWrite( LED_HEARTBEAT, LOW ); }
						break;
						
					case 3:
						if( LED_HEARTBEAT ) { digitalWrite( LED_HEARTBEAT, ! digitalRead(LED_HEARTBEAT) ); }
						break;
						
					default: break;
				}

				free( pkt.payload );
				#if __AVR_LIBC_VERSION__ < 20110216UL && defined __fix28135_h____
				fix28135_malloc_bug();
				#endif
			}
		}
	}
}