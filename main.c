#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define LEDR_BASE             0xFF200000
#define AUDIO_BASE            0xFF203040
#define KEY_BASE              0xFF200050

/* globals */
#define BUF_SIZE 512 // Around an eigth of a second, temporary for now, will work on getting more samples per second
#define BUF_THRESHOLD 32 // 25% of 128 word buffer

// Since complex.h doesn't exist have to write my own complex handling system
typedef double Re;
typedef double Im;

//global variable for pixel buffer
volatile int pixel_buffer_start; 

#define PI 3.141592654

void check_KEYs(int *, int *, int *);
void fft(Re buf_re[], Im buf_im[], int n);
double sqrt(double number);

//function prototypes for VGA display
void draw_line(int x0, int y0, int x1, int y1, short int line_color);
void clear_screen();
void plot_pixel(int x, int y, short int line_color);
void wait_for_vsync();
int x_scale(int x);
int y_scale(float y);

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
    
    /* used for vga display */
    //line colour for graph
    short int line_color = 0xF81F;
    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
    //set front pixel buffer to start of FPGA On-chip memory
    *(pixel_ctrl_ptr + 1) = 0xC8000000; 
    //swapping the front/back buffers, to set the front buffer location
    wait_for_vsync();
    //initializing a pointer to the pixel buffer, used by the  drawing functions
    pixel_buffer_start = *pixel_ctrl_ptr;

    clear_screen(); 

    // pixel_buffer_start points to the pixel buffer
    //set back pixel buffer to start of SDRAM memory 
    *(pixel_ctrl_ptr + 1) = 0xC0000000;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer

    int y_plot, x_plot;

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

         /*******************ANIMATION PART********************/
        clear_screen();
    
                //sorting frequencies into 32 bins to plot
		for(int i = 0; i < BUF_SIZE; i++){
            //frequency bin 1
            if(i<62){
                x_plot = 0;
                y_plot += y_scale(left_buffer_re[i]);
                draw_line(x_plot, y_plot, x_plot, 220, line_color);
            } 
            //frequency bin 2
            else if(62<=i && i <124){
                x_plot = 10;
                y_plot += y_scale(left_buffer_re[i]);
                draw_line(x_plot, y_plot, x_plot, 220, line_color);
            } 
            //frequency bin 3
            else if(124<=i && i <186){
                x_plot = 20;
                y_plot += y_scale(left_buffer_re[i]);
                draw_line(x_plot, y_plot, x_plot, 220, line_color);
            } 
            //frequency bin 4
            else if(186<=i && i<248){
                x_plot = 30;
                y_plot += y_scale(left_buffer_re[i]);
                draw_line(x_plot, y_plot, x_plot, 220, line_color);
            } 
            //frequency bin 5
            else if(248<=i && i<310){
                x_plot = 40;
                y_plot += y_scale(left_buffer_re[i]);
                draw_line(x_plot, y_plot, x_plot, 220, line_color);
            } 
            //frequency bin 6
            if(310<=i && i<372){
                x_plot = 50;
                y_plot += y_scale(left_buffer_re[i]);
                draw_line(x_plot, y_plot, x_plot, 220, line_color);
            } 
            //frequency bin 7
            if(372<=i && i<434){
                x_plot = 60;
                y_plot += y_scale(left_buffer_re[i]);
                draw_line(x_plot, y_plot, x_plot, 220, line_color);
            } 
            
		}
		
        wait_for_vsync(); // swap front and back buffers on VGA vertical sync
        pixel_buffer_start = *(pixel_ctrl_ptr + 1); // new back buffer
        
        //Test code
        for(int i = 0; i < BUF_SIZE; i++) {
	    double value = sqrt(left_buffer_re[i]*left_buffer_re[i] + left_buffer_im[i]*left_buffer_im[i]);
	    printf("%lf ", value);
        }
        printf("\n");
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
double pow_me(double in, int power) {
    double out = in;

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
double sin_me(double in) {
    //First reduce the input to be between 0 and pi/2
    double quotient = in;
    double quadrant = 0;

    while((quotient - PI/2) > 0) {
        quotient -= PI/2;
        quadrant += 1;

        if(quadrant == 4) quadrant = 0;
    }

    if(quadrant == 0) {
        double taylor_value = quotient - (pow_me(quotient, 3) / 6) + (pow_me(quotient, 5) / 120);
        return taylor_value;
    } else if(quadrant == 1) {
        double taylor_value = (PI/2 - quotient) - (pow_me(PI/2 - quotient, 3) / 6) + (pow_me(PI/2 - quotient, 5) / 120);
        return taylor_value;
    }
    else if(quadrant == 2) {
        double taylor_value = (quotient) - (pow_me(quotient, 3) / 6) + (pow_me(quotient, 5) / 120);
        return -taylor_value;
    }
    else {
        double taylor_value = (PI/2 - quotient) - (pow_me(PI/2 - quotient, 3) / 6) + (pow_me(PI/2 - quotient, 5) / 120);
        return -taylor_value;
    }
    
    return -1;
}

/*
 * Copied from https://stackoverflow.com/questions/11644441/fast-inverse-square-root-on-x64/11644533
 * A inv square root function, derived from an video game implementation
 * Does some weird witchcraft
 */
double sqrt(double number) {
    double y = number;
    double x2 = y * 0.5;
    long long i = *(long long *) &y;
    // The magic number is for doubles is from https://cs.uwaterloo.ca/~m32rober/rsqrt.pdf
    i = 0x5fe6eb50c7b537a9 - (i >> 1);
    y = *(double *) &i;
    y = y * (1.5 - (x2 * y * y));   // 1st iteration
    //      y  = y * ( 1.5 - ( x2 * y * y ) );   // 2nd iteration, this can be removed
    return 1/y;
}

double cos_me(double in) {
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

/*****************Helper Functions for Drawing*******************/
void draw_line(int x0, int y0, int x1, int y1, short int colour) {
	for(int i = 0; i < 5; i++){
        bool is_steep = abs(y1-y0) > abs(x1-x0);

        if(is_steep) {
            int temp = x0;
            x0 = y0;
            y0 = temp;
		
            temp = x1;
            x1 = y1;
            y1 = temp;
        } 
	
        if(x0 > x1) {
            int temp = x0;
            x0 = x1;
            x1 = temp;
		
            temp = y0;
            y0 = y1;
            y1 = temp;
        }
	
        int delta_x = x1 - x0;
        int delta_y = abs(y1 - y0);
        int error = -(delta_x / 2);
        int y = y0;

        int y_step = 1;
	
        if(y0 >= y1) y_step = -1;
	
        for(int x = x0; x < x1; x++) {
            if(is_steep){
                plot_pixel(y, x, colour);	
            } else {
                plot_pixel(x, y, colour);
            }
            error = error + delta_y;
            if(error >= 0) {
                y = y + y_step;
                error = error - delta_x;
            }
        }
        x0 ++;
        x1 ++;
    }
}

void plot_pixel(int x, int y, short int line_color)
{
    *(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;
}

void clear_screen(){
	for(int x = 0; x < 320; x++){
		for(int y = 0; y < 240; y++){
			plot_pixel(x, y, 0xFFFF);	
		}
	}
	
	//draw_line(20, 220, 320, 220, 0x0);
	//draw_line(20, 0, 20, 220, 0x0);
}

void wait_for_vsync(){
	volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
	register int status;
	
	*pixel_ctrl_ptr = 1; //writes 1 into front buffer register (starts synchro process)
	
	status = *(pixel_ctrl_ptr + 3);
	
	while((status & 0x01) != 0){
		status = *(pixel_ctrl_ptr + 3);
	}
}

// int x_scale(int x){
// 	if(x > 320) x = 320;
	
// 	return (x);
// }

int y_scale(float y){	
    if(y > 240) y = 0;                           
 	return ((int)(240 - ((240/10)*(y/10))));
}
