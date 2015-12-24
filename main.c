#define F_CPU (2*1000000UL)

#include <avr/io.h>
#include <util/delay.h>

#define US_PULSE 100
#define LVL_MAX 0x7fffffff
#define LVL_SCALE(x) ((x >> 16) << 1)
#define LVL_RAMP(x) (LVL_MAX / 100 / x)

#define PIN_ZEROCROSS PB3
#define PIN_LED       PB1
#define PIN_BUTTON    PB2
#define PIN_TRIAC     PB4



// Fast 16x16 -> high16 multiplication approximation.
// Equivalent to (a * b) >> 16  (ignoring overflow)
//
// XXX: this function is an approximation. In 25% of the cases, the 
// return value is 1 lower than it should be. Never higher.
//
// Cycle count on GCC 4.8.1 with -Os: 166 + (# of bits set in <a>)
// (add 7 for call/ret)
unsigned int mul16_h16(unsigned int a, unsigned int b)
{
	unsigned int c = 0;
	unsigned char i;

	b >>= 1;
	for (i = 0; i < 16; i++)
	{
		c >>= 1;
		if (a & 0x01)
			c += b;
		a >>= 1;
	}
	return c;
}



// Blinks the led. Call once every semicycle. The parameters indicate
// how many semicycles the led should be on, and how many off (254 MAX).
// E.g. blink(50, 50) is 0.5 sec on, 0.5 sec off (assuming 50Hz)
// blink(10, 90) is 0.1 sec on, 0.9 off. etc.
void blink(unsigned char on, unsigned char off)
{
	static unsigned char state = 0;
	static unsigned char count = 0;

	count++;

	if (state == 0 && count > off && on > 0)
	{
		PORTB |= _BV(PIN_LED);
		state = 1;
		count = 0;
		return;
	}

	if (state == 1 && count > on && off > 0)
	{
		PORTB &= ~_BV(PIN_LED);
		state = 0;
		count = 0;
		return;
	}
}



// Returns true iff the button is pressed and was depressed the previous 7
// invocations of this function. If polled every semicycle, this provides
// good debouncing.
unsigned char button_pressed(void)
{
	static unsigned char acc = 0;

	acc <<= 1;
	if (PINB & _BV(PIN_BUTTON))
		acc |= 1;

	return acc == 0x01;
}



// They "main loop" function, which waits out the rest of the semicycle,
// triggering the triac at an appropriate moment (as indicated by level).
// Call this function, do a little processing, call it again, etc, etc.
// Try to keep time between calls as constant as possible.
// Level indicates brightness, ranging from 0 to 0xffff.
// Actual resolution appears to be something like 1 in 1500.
void cycle(unsigned int level)
{
	// Keep track of which semicycle we're dealing with.
	// 0xff for the lower semicycle, 0x00 for the upper half.
	static unsigned char which = 0x00;
	// The mains cycle length in loop counts. About 1500-ish.
	static unsigned int cycle_len = 0xffff;
	// Loop counter for the current iteration.
	unsigned int count = 0;
	// Target loop counts after which to trigger the triac.
	unsigned int target = 0;



	// Implement a level scaling that starts out linear but
	// tends to hypercubed eventually. This is a lot closer to
	// perceived linear brightness due to the way incandescents
	// and AC sine waves interact. (abuses target as temp var)
	target = mul16_h16(level, level);
	target = mul16_h16(target, target);
	target = (target >> 1) + (level >> 1);

	// Invert level; a low level translates to a high target
	target = 0xffffU - target;
	// We pretend the cycle is 128 counts shorter than it really is, to
	// prevent overshoot. About 0x40 seems really needed to prevent
	// incorrect triac triggering. The larger value makes level=0 close to
	// the glow point of the halogens I use.
	target = mul16_h16(cycle_len - 0x90, target);

	// At lowest level, set an unreachable target to ensure the triac
	// never gets fired. (count seems to run up to 1500-ish)
	// The expensive target calculation is discarded in this case, but we
	// want the computations to always happen to avoid runtime variations.
	if (level == 0)
		target = 0xffffU;



	// Because of the way this zero-cross detection works, the upper
	// semicycle appears longer by about 1%.  We compensate for this by
	// delaying for a bit before the main loop of the lower semicycle.
	// This ensures <count> is roughly the same for both semis.
	// 63us empirically found to be most stable.
	if (!which)
		_delay_us(63);



	count = 0;
	// Bitwise I/O accumulator, for debouncing.
	unsigned char io_acc = ~which;
	// Copy of <which> in a non-static variable. Otherwise GCC fails to
	// realize it doesn't have to fetch <which> from SRAM every iteration.
	unsigned char io_tgt = which;
	// Actual phase timing loop starts here.
	while (io_acc != io_tgt)
	{
		io_acc <<= 1;
		if (PINB & _BV(PIN_ZEROCROSS))
			io_acc |= 1;

		if (count == target)
		{
			PORTB &= ~_BV(PIN_TRIAC);
			_delay_us(US_PULSE);
			PORTB |= _BV(PIN_TRIAC);
			// Count compensation because of pulse delay.
			// One count should be 13 cycles.
			// One pulse is 100us, or 200 cycles at 2 MHz.
			// So, compensate by 200 / 13 = 15.
			count += 15;
		}

		count++;
	}

	// Start-up hack.
	// <cycle_len> starts out at 0xffff. <count> will be 1500-ish, so
	// <cycle_len> gets decreased each iteration. After 4 iterations,
	// it reaches 0xfffb, triggering a one-time shortcut to <count>.
	if (cycle_len == 0xfffb)
		cycle_len = count;

	// Adjust cycle_len, if needed. Adjusting by one step max prevents
	// intererence from messing things up too badly.
	if (count > cycle_len)
		cycle_len++;
	if (count < cycle_len)
		cycle_len--;

	// Select the other semicycle.
	which = ~which;
}



int main (void)
{
	unsigned long level;

	// Set clock prescaler to 256, resulting in a ~32KHz clock.
	// This minimizes power consumption.
	CLKPR = 0x80;
	CLKPR = 0x08;

	// Wait a little. At minimal power consumption, this allows the power
	// supply to stabilize. At 1/64th clock, this actually delays ~64ms.
	_delay_ms(1);

	// Set clock prescaler to 4, resulting in a ~2MHz clock.
	// This is our actual working frequency.
	CLKPR = 0x80;
	CLKPR = 0x02;

	// Set outputs
	PORTB |= _BV(PIN_TRIAC); // High = inactive
	DDRB = _BV(PIN_TRIAC) | _BV(PIN_LED);

	// Synchronize to the AC sine wave.
	cycle(0);
	cycle(0);



	// Ramp up
	for (level = 0; level <= LVL_MAX; level += LVL_RAMP(1800))
	{
		cycle(LVL_SCALE(level));
		blink(5, 15);
		if (button_pressed())
			goto off;
	}

	// Steady
	for (level = 0; level <= LVL_MAX; level += LVL_RAMP(2700))
	{
		cycle(0xffffU);
		blink(1, 0);
		if (button_pressed())
		{
			level = LVL_MAX;
			goto off;
		}
	}

	// Ramp down
	for (level = LVL_MAX; level <= LVL_MAX; level -= LVL_RAMP(2))
	{
		cycle(LVL_SCALE(level));
		blink(15, 5);
	}

off:
	// Off
	for (; level <= LVL_MAX; level -= LVL_RAMP(0.5))
	{
		cycle(LVL_SCALE(level));
		blink(5, 5);
		if (button_pressed())
			goto on;
	}
	level = 0;
	for (;;)
	{
		cycle(0);
		blink(5, 95);
		if (button_pressed())
			goto on;
	}

on:
	// On (only reachable by button)
	for (; level <= LVL_MAX; level += LVL_RAMP(1))
	{
		cycle(LVL_SCALE(level));
		blink(5, 5);
		if (button_pressed())
			goto off;
	}
	level = LVL_MAX;
	for (;;)
	{
		cycle(0xffffU);
		blink(95, 5);
		if (button_pressed())
			goto off;
	}

}
