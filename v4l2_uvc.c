#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <sys/mman.h>

#define CAMERA_DEV "/dev/video0"
#define NB_BUFFER	16
#define PIC_NUM		30
#define PIC_WIDTH	640	//320
#define PIC_HEIGHT	480	//240
#define FPS		30

#define YUV2RGB
#define CLEAR(x) memset (&(x), 0, sizeof (x))


struct buffer {
	void *      start;
	size_t      length;
};

typedef struct {
	char *data;
	unsigned long length;
} frame_t;


struct buffer *         buffers		= NULL;
static unsigned int     n_buffers       = 0;
static int		camera_fd	= -1;


unsigned int convert_yuv_to_rgb_pixel(int y, int u, int v)
{
    unsigned int pixel32 = 0;
    unsigned char *pixel = (unsigned char *)&pixel32;
    int r, g, b;

    r = y + (1.370705 * (v-128));
    g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
    b = y + (1.732446 * (u-128));
    if(r > 255) r = 255;
    if(g > 255) g = 255;
    if(b > 255) b = 255;
    if(r < 0) r = 0;
    if(g < 0) g = 0;
    if(b < 0) b = 0;
    pixel[0] = r ;
    pixel[1] = g ;
    pixel[2] = b ;

    return pixel32;
}

unsigned int convert_yuv_to_rgb_buffer(unsigned char *yuv,
					unsigned char *rgb,
					unsigned int width,
					unsigned int height)
{
	unsigned int in, out = 0;
	unsigned int pixel_16;
	unsigned char pixel_24[3];
	unsigned int pixel32;
	int y0, u, y1, v;

	for (in = 0; in < width * height * 2; in += 4) {
		pixel_16 =	yuv[in + 3] << 24 |
				yuv[in + 2] << 16 |
				yuv[in + 1] <<  8 |
				yuv[in + 0];

		y0 = (pixel_16 & 0x000000ff);
		u  = (pixel_16 & 0x0000ff00) >>  8;
		y1 = (pixel_16 & 0x00ff0000) >> 16;
		v  = (pixel_16 & 0xff000000) >> 24;

		pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
		pixel_24[0] = (pixel32 & 0x000000ff);
		pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
		pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;

		rgb[out++] = pixel_24[0];
		rgb[out++] = pixel_24[1];
		rgb[out++] = pixel_24[2];

		pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
		pixel_24[0] = (pixel32 & 0x000000ff);
		pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
		pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;

		rgb[out++] = pixel_24[0];
		rgb[out++] = pixel_24[1];
		rgb[out++] = pixel_24[2];
	}

	return 0;
}

int init_device (int fd, int width, int height, int fps)
{
	struct v4l2_requestbuffers req;
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_fmtdesc fmtdesc;
	unsigned int min;
	int ret;
	int i;

	if (-1 == ioctl (fd, VIDIOC_QUERYCAP, &cap)) {
		fprintf (stderr, "VIDIOC_QUERYCAP fail\n");
		goto err;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf (stderr, "device does not support video capture\n");
		goto err;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf (stderr, "device does not support streaming i/o\n");
		goto err;
	}

	/* Print capability infomations */
	printf("\nCapability Informations:\n");
	printf(" driver: %s\n", cap.driver);
	printf(" card: %s\n", cap.card);
	printf(" bus_info: %s\n", cap.bus_info);
	printf(" version: %08X\n", cap.version);

	/* Select video input, video standard and tune here. */
	CLEAR (cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == ioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == ioctl (fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
				case EINVAL:
					/* Cropping not supported. */
					break;
				default:
					/* Errors ignored. */
					break;
			}
		}
	}

	/* enum video formats. */
	CLEAR(fmtdesc);
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	printf("\nEnum format:\n");
	//while ((ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) == 0)
	while ((ret = v4l2_ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) == 0)
	{
		fmtdesc.index++;
		printf(" <%d> pixelformat = \"%c%c%c%c\", description = %s\n",
			fmtdesc.index,
			fmtdesc.pixelformat & 0xFF,
			(fmtdesc.pixelformat >> 8) & 0xFF,
			(fmtdesc.pixelformat >> 16) & 0xFF,
			(fmtdesc.pixelformat >> 24) & 0xFF,
			fmtdesc.description);
	}

	/* set video formats. */
	CLEAR (fmt);
	char * p = (char *)(&fmt.fmt.pix.pixelformat);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl (fd, VIDIOC_G_FMT, &fmt) < 0) {
		/* Errors ignored. */
		printf("get fmt fail\n");
	}

	fmt.fmt.pix.width       = width;
	fmt.fmt.pix.height      = height;
	//    init_mem_repo();
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

	if (-1 == ioctl (fd, VIDIOC_S_FMT, &fmt)){
		printf("set format fail\n");
		goto err;
	}

	if (ioctl (fd, VIDIOC_G_FMT, &fmt) < 0) {
		/* Errors ignored. */
		printf("get fmt fail\n");
	}

	printf("\n\n");
        printf("fmt.type = %d\n", fmt.type);
        printf("fmt.width = %d\n", fmt.fmt.pix.width);
        printf("fmt.height = %d\n", fmt.fmt.pix.height);
        printf("fmt.format = %c%c%c%c\n", p[0], p[1], p[2], p[3]);
        printf("fmt.field = %d\n", fmt.fmt.pix.field);
        printf("fps = %d\n\n", fps);

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;

	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	struct v4l2_streamparm* setfps;
	setfps=(struct v4l2_streamparm *) calloc(1, sizeof(struct v4l2_streamparm));
	memset(setfps, 0, sizeof(struct v4l2_streamparm));
	setfps->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	setfps->parm.capture.timeperframe.numerator = 1;
	setfps->parm.capture.timeperframe.denominator = fps;
	if(ioctl(fd, VIDIOC_S_PARM, setfps) < 0){
		printf("set fps fail\n");
		goto err;
	}

	CLEAR (req);
	req.count   = NB_BUFFER;
	req.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory  = V4L2_MEMORY_MMAP;

	if (-1 == ioctl (fd, VIDIOC_REQBUFS, &req)) {
		fprintf (stderr, "VIDIOC_QUERYCAP fail\n");
		goto err;
	}

	if (req.count < 2) {
		fprintf (stderr, "Insufficient buffer memory\n");
		goto err;
	}

	buffers = calloc (req.count, sizeof (*buffers));
	if (!buffers) {
		fprintf (stderr, "buffers, Out of memory\n");
		goto err;
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;

		CLEAR (buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers;

		if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buf)) {
			fprintf (stderr, "VIDIOC_QUERYCAP fail\n");
			goto err2;
		}

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start = mmap (NULL /* start anywhere */,
						buf.length,
						PROT_READ | PROT_WRITE /* required */,
						MAP_SHARED /* recommended */,
						fd,
						buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start) {
			fprintf (stderr, "mmap fail\n");
			goto err2;
		}
	}

	return 0;

err2:
	if (n_buffers > 0) {
		for (i = (n_buffers-1); i >= 0; i--) {
			munmap(buffers[i].start, buffers[i].length);
		}
	}

	free(buffers);
err:
        return -1;
}

static void errno_exit (const char *s)
{
	fprintf (stderr, "%s error %d, %s\n",s, errno, strerror (errno));
	exit (EXIT_FAILURE);
}

void start_capturing (int fd)
{
	unsigned int i,ret;
	enum v4l2_buf_type type;

	printf("n_buffers:%d\n", n_buffers);
	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;

		CLEAR (buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;
		ret = ioctl (fd, VIDIOC_QBUF, &buf);
		if (-1 == ret)
			errno_exit ("VIDIOC_QBUF");
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl (fd, VIDIOC_STREAMON, &type);
	if (-1 == ret) {
		errno_exit ("VIDIOC_STREAMON");
	}
}

/*
 * read a frame of image from video device
 */
int get_buffer(int fd, struct v4l2_buffer *v4l_buf)
{
	if (-1 == ioctl (fd, VIDIOC_DQBUF, v4l_buf))
		return 0;

	return 1;
}

/*
 * enqueue the frame again
 */
int put_buffer(int fd, struct v4l2_buffer *v4l_buf)
{
	return ioctl(fd, VIDIOC_QBUF, v4l_buf);
}

int main(void)
{
	int fd ,fd1 = 0;
	int ret,r,i;
	struct timeval tv;
	fd_set fds;
	char file[20];
	frame_t *fm;
	struct v4l2_buffer buf = {0};
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	fm = malloc(sizeof(frame_t));
	if (!fm) {
		printf("fm, Out of memory\n"); 
		return -1;
	}

#ifdef YUV2RGB
	FILE *outfile_fd;
	unsigned char* yuv;
	unsigned char* rgb;

	rgb = malloc(PIC_WIDTH*PIC_HEIGHT*3);
	if (!rgb) {
		printf("rgb, Out of memory\n");
		ret = -1;
		goto err_rgb_malloc;
	}
#endif

	//fd  = open (CAMERA_DEV, O_RDWR /* required */ | O_NONBLOCK, 0); 
	fd = v4l2_open(CAMERA_DEV, O_RDWR);
	if(fd < 0) {
		printf("open camera failed\n");    
		close(fd);
		ret = -1;
		goto err_open_cam;
	}

	camera_fd = fd;

	printf("\ninit device\n");
	init_device (fd, PIC_WIDTH, PIC_HEIGHT, FPS);
	//close(fd);
	//return 0;

	printf("start capturing\n");
	start_capturing (fd);

	for (i=0; i<PIC_NUM; i++) {
		FD_ZERO (&fds);
		FD_SET (fd, &fds);
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select (fd + 1, &fds, NULL, NULL, &tv);
		//printf("r1= %d\n",r);
		r = get_buffer(fd, &buf);
		//printf("r2= %d\n",r);
		fm->data = buffers[buf.index].start;
		fm->length = buf.bytesused;

		if(r!=0) {

#ifdef YUV2RGB
			sprintf(file,"./pic/pic%d.raw",i);
			yuv = fm->data;
			memset(rgb, 0, PIC_WIDTH*PIC_HEIGHT*3);

			//printf("before yuv2rgb\n");
			convert_yuv_to_rgb_buffer(yuv, rgb, PIC_WIDTH, PIC_HEIGHT);
			//printf("after yuv2rgb\n");

			outfile_fd = fopen(file, "w");
			printf("write %s\n", file);
			fwrite(rgb, PIC_WIDTH*PIC_HEIGHT*3, 1, outfile_fd);
			fclose(outfile_fd);
#else
			sprintf(file,"./pic/pic%d.jpg",i);
			fd1 = open(file, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
			if(fd1) {
				printf("open wch and write %s\n", file);
				write(fd1, fm->data, fm->length);
				close(fd1);
			}
#endif

		}

		r = put_buffer(fd, &buf);
		//printf("r3= %d\n",r);
	}

	close(fd);
	ret = 0;

err_open_cam:
#ifdef YUV2RGB
	free(rgb);
#endif

err_rgb_malloc:
	free(fm);

	return ret;    
}
