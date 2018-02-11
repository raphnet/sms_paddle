/* DIY SMS/MARKIII Paddle
 * Copyright (C) 2015 Raphaël Assénat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The author may be contacted at raph@raphnet.net
 */
#include <avr/io.h>
#include <util/delay.h>

#define EXPORT_PADDLE_SUPPORT	1

#define STATE_HPD_INIT			1
#define STATE_HPD_RUNNING		2
#define STATE_EXPORT_INIT		3
#define STATE_EXPORT_RUNNING	4

int main(void)
{
	unsigned char value = 0x00;
	unsigned char state = STATE_HPD_INIT;

	/* PORTB
	 * 0: Output low (mode switch)
	 * 1: Input with pull up (mode switch)
	 * 2-7: Input with pull up (unused)
	 */
	DDRB = 0x01;
	PORTB = 0xFE;

	/*
	 * PORTD
	 *
	 * 2: Input for export paddle
	 */
	DDRD = 0x00;
	PORTD = 0xFF;


	/* PORTC
	 *
	 * 0: ADC0 input
	 * 1: TR output
	 * 2: D0/D4 output
	 * 3: D1/D5 output
	 * 4: D2/D6 output
	 * 5: D3/D7 output
	 * 6: /RESET
	 */
	DDRC = 0xFE;
	PORTC = 0x00;

#ifdef EXPORT_PADDLE_SUPPORT
	_delay_ms(10);
	// PB1 is low when the switch is closed. This enables "Export paddle" mode
	if (!(PINB & 0x02)) {
		state = STATE_EXPORT_INIT;
	}
#endif

	ADMUX = (1<<ADLAR) | (1<<REFS0); // AVCC reference, channel 0 (ADC0), left adjusted result

	while(1)
	{
		switch (state)
		{
			default:
				state = STATE_HPD_INIT;
				break;

			case STATE_HPD_INIT:
				// prescaler set to /32
				// 8MHz / 32 = 250kHz. 13 ADC clocks are required for a conversion,
				// s 250kHz / 13 = ~19.23 kHz. (Periode of ~52uS)
				ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS0);
				state = STATE_HPD_RUNNING;
				break;

			case STATE_HPD_RUNNING:
				ADCSRA |= (1<<ADSC);
				while (ADCSRA & (1<<ADSC)) { }

				value = ADCH;
				value ^= 0xff;

				PORTC = (value << 2) & 0x3c;
				_delay_us(62);
				//_delay_us(124);

				PORTC = ((value >> 2) & 0x3c) | 0x02;
				//_delay_us(72); // 124us - 52us (to compensate time spent in ADC)
				_delay_us(10); // 124us - 52us (to compensate time spent in ADC)
				break;

			case STATE_EXPORT_INIT:
				// Switch ADC to free running mode so we can concentrate on reacting
				// quickly to transition on PORTD2
				ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS0);
#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega168A__) || defined(__AVR_ATmega168P__)
				ADCSRB = 0; // Auto-trigger source: Free Running mode
				ADCSRA |= (1<<ADATE) | (1<<ADSC);
#else
				ADCSRA |= (1<<ADFR) | (1<<ADSC);
#endif
				state = STATE_EXPORT_RUNNING;
				break;

			case STATE_EXPORT_RUNNING:

				PORTC = ((value >> 2) & 0x3c) | 0x02;
				while (PIND & 0x04) { }

				// New value on falling edge
				value = ADCH;
				value ^= 0xff;

				PORTC = (value << 2) & 0x3c;
				while (!(PIND & 0x04)) { }
				break;
		}
	}

#if 0
	/* The export paddle is supposed to follow transitions on pin 7.
	 * But for test purposes when this pin is not wired, the hack
	 * below only sends the upper nibble. So the only values sent
	 * are 0x11, 0x22, ... 0xee, 0xff.
	 *
	 * This works, but the controller is not very usable
	 * (only 16 directions instead of 256).
	 *
	 */
	while(!export_paddle_mode)
	{
		ADCSRA |= (1<<ADSC);
		while (ADCSRA & (1<<ADSC)) { }

		value = ADCH;

		PORTC = ((value >> 2) & 0x3c);
		_delay_us(124);

		PORTC = ((value >> 2) & 0x3c) | 0x02;
		_delay_us(72); // 124us - 52us (to compensate time spent in ADC)
	}
#endif
}
