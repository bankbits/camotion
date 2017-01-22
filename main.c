#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include "imgproc.h"
#include "pil_io.h"
#include "pil.h"

#define WIDTH 640
#define HEIGHT 480
#define BUFFER_SIZE 30

int MilliTime()
{
	int iTime;
	struct timespec res;

	clock_gettime(CLOCK_MONOTONIC, &res);
	iTime = 1000*res.tv_sec + res.tv_nsec/1000000;
	
	return iTime;
} /* MilliTime() */

int kbhit(void)
{
struct timeval tv;
fd_set rdfs;

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&rdfs);
	FD_SET(STDIN_FILENO, &rdfs);
	select(STDIN_FILENO+1, &rdfs, NULL, NULL, &tv);
	return FD_ISSET(STDIN_FILENO, &rdfs);
} /* kbhit() */

// find frame with least noise
int MinFrame(int rcap[], int gcap[], int bcap[], int size) 
{
	int i;
	float rmin, gmin, bmin;
	rmin = rcap[0];
	gmin = gcap[0];
	bmin = bcap[0];
	int index = 0;

	if(rcap[size - 1] != NULL) 
	{
		for(i = 0; i < size; i++) 
		{
			if(rcap[size] < rmin || gcap[size] < gmin || bcap[size] < bmin) 
			{
				rmin = rcap[size];
				gmin = gcap[size];
				bmin = bcap[size];
				index = i;
			}
		}
	}
	else {
		printf("rcap[30] is NULL\n");
		return -1;
	}

	printf("Min frame called.\n");
	return index;
} /* MinFrame() */

int main(int argc, char * argv[])
{
	int fbfd = 0;
	int iTime;
	float fps; // frames per sec
	int frames = 0;
	int buffer_count = 0; // counter for circular buffer
	int start_frame = 0; // frame num where motion starts

	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	long int screensize = 0;
	char *fbp = 0;

	Camera * cam;
	Image * img;
	Image * img_prev;
	Image * best_frame;
	Image * cap_frames[BUFFER_SIZE]; // circular buffer capture frames over 30 seconds

	int rcap[BUFFER_SIZE];
	int gcap[BUFFER_SIZE];
	int bcap[BUFFER_SIZE];

	int rc, i;
	PIL_PAGE pp,pp2;
	PIL_FILE pf;
	
	int x, y;
	int r, g, b;
	int r1 = 0;
	int g1 = 0; 
	int b1 = 0;

	float rw, gw, bw; // weights for r, g, b
	rw = 1;
	gw = 0;
	bw = 1;

	unsigned char *pImage, *pImage_prev, *ps, *ps_prev;
	unsigned char *pScreen, *pd;

	
	// CAMERA CODE

	// open the webcam, with a capture resolution of width 320 and height 240
	cam = camOpen(WIDTH, HEIGHT);
printf("camOpen returned cam structure %08x\n", (int)cam);
	// FRAMEBUFFER CODE

	  // Open the file for reading and writing
	  fbfd = open("/dev/fb0", O_RDWR);
	  if (!fbfd) {
	    printf("Error: cannot open framebuffer device.\n");
	    return(1);
	  }
	  printf("The framebuffer device was opened successfully.\n");
	
	  // Get fixed screen information
	  if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
	    printf("Error reading fixed information.\n");
	  }
	
	  // Get variable screen information
	  if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
	    printf("Error reading variable information.\n");
	  }
	  printf("%dx%d, %d bpp\n", vinfo.xres, vinfo.yres, 
	         vinfo.bits_per_pixel );
	
	  // map framebuffer to user memory 
	  screensize = finfo.smem_len;
	
	  fbp = (char*)mmap(0, 
	                    screensize, 
	                    PROT_READ | PROT_WRITE, 
	                    MAP_SHARED, 
	                    fbfd, 0);
	
	  if ((int)fbp == -1) {
	    printf("Failed to mmap.\n");
	  }
	  else {
		pScreen = fbp;
		
		iTime = MilliTime();
		img = NULL;
		while(!kbhit()) 
		{
			img_prev = img;
			img = camGrabImage(cam);
			if (img == NULL || img->data == NULL) {printf("cam returned NULL!\n"); return 0;}
			pImage = img->data;
			if (img_prev != NULL) pImage_prev = img_prev->data;
			r1 = g1 = b1 = 0;
			

			// Display frame on screen using framebuffer
			for(y = 0; y < HEIGHT; y++) 
			{
				ps = &pImage[y * img->width*3];
				ps_prev = &pImage_prev[y * img->width*3];
				pd = &pScreen[y * vinfo.xres * 4];
	
				for(x = 0; x < WIDTH; x++) 
				{
					// bit order with least significant bit first (blue value)
					b = ps[0];
					g = ps[1];
					r = ps[2];
					pd[0] = b;
					pd[1] = g;
					pd[2] = r;
					pd[3] = 255;	
					if (img_prev != NULL)
					{
						b1 += abs(b - ps_prev[0]);
						g1 += abs(g - ps_prev[1]);
						r1 += abs(r - ps_prev[2]);
					}
					ps += 3;
					ps_prev += 3;
					pd += 4;
				} // for x
			} // for y

			cap_frames[buffer_count] = img;

			bcap[buffer_count] = b1;			
			gcap[buffer_count] = g1;
			rcap[buffer_count] = r1;

			if(frames == (start_frame + BUFFER_SIZE)) 
			{
				// save the best frame from the buffer
				printf("%d\n", MinFrame(rcap, gcap, bcap, BUFFER_SIZE));
				best_frame = cap_frames[MinFrame(rcap, gcap, bcap, BUFFER_SIZE)]; 
				printf("BEST FRAME SAVED\n");
			}


//			printf("R: %d\t G: %d\t B: %d\n", r1, g1, b1);

			if(((float)r1*rw) >= 2000000.0 || ((float)g1*gw) >= 2000000.0 || ((float)b1*bw) >= 2000000.0) 
			{
				printf("MOTION TRIGGERED\n");
				
				if((frames - start_frame) >= BUFFER_SIZE)
					start_frame = frames;

				// Motion detected, now display current frame next to live captured frames
				for(y = 0; y < HEIGHT; y++) 
				{
					ps = &pImage[y * img->width*3];
					pd = &pScreen[y * vinfo.xres * 4];
		
					for(x = 0; x < WIDTH; x++) 
					{
						b = ps[0];
						g = ps[1];
						r = ps[2];
						pd[(640 * 4)] = b;
						pd[(640 * 4) + 1] = g;
						pd[(640 * 4) + 2] = r;
						pd[(640 * 4) + 03] = 255;	
						
						ps += 3;
						pd += 4;
					} // for x
				} // for y
				
			}

			if (img_prev != NULL)
				imgDestroy(img_prev);

			frames++;
			if(buffer_count > 29)
				buffer_count = 0;

			buffer_count++;	
		}
		imgDestroy(img); // free last frame
		iTime = MilliTime() - iTime;
	
		// get framerate
		fps = (float) iTime;
		fps /= 1000.0;
		printf("Time (s): %.2f\n", fps);
		printf("Total frames: %d\n", frames);
		fps = (float)frames / fps;
		printf("Frames per second: %.2f\n", fps);
		
	 }

// now we will free the memory for the various objects

	// finally quit
	camClose(cam);

	// finally we uninitialise the library
	quit_imgproc();

	// close the framebuffer
	munmap(fbp, screensize);
 	close(fbfd);

	return 0;
} /* main() */
