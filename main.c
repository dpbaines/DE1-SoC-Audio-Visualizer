#include <math.h>

#define LEDR_BASE             0xFF200000
#define AUDIO_BASE            0xFF203040
#define KEY_BASE              0xFF200050

/* globals */
#define BUF_SIZE 512 // Around an eigth of a second, temporary for now, will work on getting more samples per second
#define BUF_THRESHOLD 32 // 25% of 128 word buffer

// Since complex.h doesn't exist have to write my own complex handling system
typedef float Re;
typedef float Im;

#define PI 3.141592654

void check_KEYs(int *, int *, int *);
void fft(Re buf_re[], Im buf_im[], int n);

int main(void) {
    /* Declare volatile pointers to I/O registers (volatile means that IO load
    and store instructions will be used to access these pointer locations,
    instead of regular memory loads and stores) */
    volatile int * red_LED_ptr = (int *)LEDR_BASE;
    volatile int * audio_ptr = (int *)AUDIO_BASE;
    /* used for audio record/playback */
    int fifospace;
    Re left_buffer_re[BUF_SIZE];
    Im left_buffer_im[BUF_SIZE];
    
    Re right_buffer_re[BUF_SIZE];
    Im right_buffer_im[BUF_SIZE];

    while (1) {

        //This segment loads the audio buffers
        *(red_LED_ptr) = 0x1; // turn on LEDR[0]
        fifospace = *(audio_ptr + 1); // read the audio port fifospace register
        int buffer_index = 0;
        if ((fifospace & 0x000000FF) > BUF_THRESHOLD) {
            // store data until the the audio-in FIFO is empty or the buffer
            // is full
            while ((fifospace & 0x000000FF) && (buffer_index < BUF_SIZE)) {
                left_buffer_re[buffer_index] = (Re) *(audio_ptr + 2);
                left_buffer_im[buffer_index] = 0;
                right_buffer_re[buffer_index] = (Re) *(audio_ptr + 3);
                right_buffer_im[buffer_index] = 0;
                ++buffer_index;
                if (buffer_index == BUF_SIZE) {
                    // done recording
                    *(red_LED_ptr) = 0x0; // turn off LEDR
                }
                fifospace = *(audio_ptr + 1); // read the audio port fifospace register
            }
        }

        // Use Left channel
        fft(left_buffer_re, left_buffer_im, BUF_SIZE);
    }
}

/****************************************************************************************
* Subroutine to read KEYs
* Carry over from example code I never deleted
* Might come in useful later otherwise delete
****************************************************************************************/
void check_KEYs(int * KEY0, int * KEY1, int * counter) {
    volatile int * KEY_ptr = (int *)KEY_BASE;
    volatile int * audio_ptr = (int *)AUDIO_BASE;
    int KEY_value;
    KEY_value = *(KEY_ptr); // read the pushbutton KEY values
    while (*KEY_ptr); // wait for pushbutton KEY release

    if (KEY_value == 0x1) {
        // reset counter to start recording
        *counter = 0;
        // clear audio-in FIFO
        *(audio_ptr) = 0x4;
        *(audio_ptr) = 0x0;
        *KEY0 = 1;
    } 
    else if (KEY_value == 0x2) {
        // reset counter to start playback
        *counter = 0;
        // clear audio-out FIFO
        *(audio_ptr) = 0x8;
        *(audio_ptr) = 0x0;
        *KEY1 = 1;
    }
}

//Goddammit I need to redefine pow too
float pow_me(float in, float power) {
    float out = in;

    if(power == 0) return 1;

    for(int i = 0; i < (power - 1); i++) {
        out = out * in;
    }

    return out;
}

// Just a 5th order taylor series approximation, if it's too slow there are faster algorithms out there
// It's accurate enough for visual use
// I hope to figure out the STD library
// Improvement: Use BKM or CORDIC algorithms
float sin_me(float in) {
    //First reduce the input to be between 0 and pi/2
    float quotient = in;
    float quadrant = 0;

    while((quotient - PI/2) > 0) {
        quotient -= PI/2;
        quadrant += 1;

        if(quadrant == 4) quadrant = 0;
    }

    if(quadrant == 0) {
        float taylor_value = quotient - (pow_me(quotient, 3) / 6) + (pow_me(quotient, 5) / 120);
        return taylor_value;
    } else if(quadrant == 1) {
        float taylor_value = (PI/2 - quotient) - (pow_me(PI/2 - quotient, 3) / 6) + (pow_me(PI/2 - quotient, 5) / 120);
        return taylor_value;
    }
    else if(quadrant == 2) {
        float taylor_value = (quotient) - (pow_me(quotient, 3) / 6) + (pow_me(quotient, 5) / 120);
        return -taylor_value;
    }
    else {
        float taylor_value = (PI/2 - quotient) - (pow_me(PI/2 - quotient, 3) / 6) + (pow_me(PI/2 - quotient, 5) / 120);
        return -taylor_value;
    }
    
    return -1;
}

float cos_me(float in) {
    return sin_me(in + PI/2);
}

inline Re cexp_re(Re re_in) {
    return cos_me(re_in);
}

inline Im cexp_im(Im im_in) {
    return sin_me(im_in);
}

/*
 * Original code copied from https://rosettacode.org/wiki/Fast_Fourier_transform#C
 * Modified to work without complex library which isn't supported
 */

void _fft(Re buf_re[], Im buf_im[], Re out_re[], Im out_im[], int n, int step) {
	if (step < n) {
		_fft(out_re, out_im, buf_re, buf_im, n, step * 2);
		_fft(out_re + step, out_im + step, buf_re + step, buf_im + step, n, step * 2);
 
		for (int i = 0; i < n; i += 2 * step) {
            //This conversion should work
			Re re_t = cexp_re(-PI * i / n) * out_re[i + step] - cexp_im(-PI * i / n) * out_im[i + step];
            Im im_t = cexp_im(-PI * i / n) * out_re[i + step] + cexp_re(-PI * i / n) * out_im[i + step];
			buf_re[i / 2]     = out_re[i] + re_t;
            buf_im[i / 2]     = out_im[i] + im_t;
			buf_re[(i + n)/2] = out_re[i] - re_t;
            buf_im[(i + n)/2] = out_im[i] - im_t;
		}
	}
}

/*
 *  Fast Fourier Transform function
 */
void fft(Re buf_re[], Im buf_im[], int n) {
	Re out_re[n];
    Im out_im[n];
	for (int i = 0; i < n; i++) {
        out_re[i] = buf_re[i];
        out_im[i] = buf_im[i];
    }
 
	_fft(buf_re, buf_im, out_re, out_im, n, 1);
}