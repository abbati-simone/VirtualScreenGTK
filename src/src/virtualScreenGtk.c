/*To compile:
gcc `pkg-config --cflags gtk+-3.0` virtualScreenGtk.c -o gtk `pkg-config --libs gtk+-3.0` -Wall -Wextra -pedantic */
#include <gtk/gtk.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/time.h>

/*
Binary print:
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
    (byte & 0x80 ? '1' : '0'), \
    (byte & 0x40 ? '1' : '0'), \
    (byte & 0x20 ? '1' : '0'), \
    (byte & 0x10 ? '1' : '0'), \
    (byte & 0x08 ? '1' : '0'), \
    (byte & 0x04 ? '1' : '0'), \
    (byte & 0x02 ? '1' : '0'), \
    (byte & 0x01 ? '1' : '0')
Use:
printf("16 bit "BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(secondColor16>>8), BYTE_TO_BINARY(secondColor16));
*/

#define DSTBPP 3
#define HAS_ALPHA FALSE

#define FIFO_NAME_SIZE  512

enum error {
    ER_OK = 0, ER_FILE, ER_XML, ER_OPTIONS, ER_MEMORY, ER_FIFO_OPEN
};

static char *progname;

guint8 *srcBuffer;
guint8 *dstBuffer;

FILE *f;
char *basepath;
char *fullpath;
short saveScreenshot = 0;
//int fd, fc;

int frameBytes = 0;
long long currentFrame = 0;
int Width = 640, Height = 480;
short showTrace = 0;
int msPerFrame = 40;
int srcBPP = 3;
int firstBitPP, secondBitPP, thirdBitPP;
char colorSequence[4] = "RGB";

GtkWidget *window;
GtkWidget *mainBox;
GtkWidget*image;
GtkWidget*refreshBtn;
GdkPixbuf *pixbuf;

/*static void exstatus() {
    fprintf(stderr, "\n" PACKAGE_NAME ": done. Exiting with return value %d (%s)\n", last_exit, errors_string[last_exit]);
   }*/

static void usage(void)
{
    /*fprintf(stderr, PACKAGE_NAME " Version " PACKAGE_VERSION
       "\nReleased under GNU GPL. See 'man' page for more info.\n"
       "\nUsage: %s xmlfile|-\n"
            "\t--help|-h\n",
        progname);*/
    fprintf(stderr, "\nUsage: %s -f /path/to/fifo -x WIDTH -y HEIGHT [-m FORMAT] [-p FPS] [-t] [-s /path/to/screenshots/dir] [--help|-h]"
                    "\n\nFor example:"
                    "\n\tshell 1 -> mkfifo ~/fifo"
                    "\n\tshell 1 -> %s -f ~/fifo -x 640 -y 480 -p 25 -m RGB_4_4_4"
                    "\n\tshell 2 -> cat /dev/urandom > ~/fifo\n"
                    , progname, progname);
    exit(ER_OPTIONS);
}

static void convert_color(guint8 *r, guint8 *g, guint8 *b, guint8 *rgb)
{
    guint8 firstColor, secondColor, thirdColor;
    if(srcBPP == 3) {
        firstColor = rgb[0];
        secondColor = rgb[1];
        thirdColor = rgb[2];
    }
    else if(srcBPP == 2) {
        guint16 firstColor16, secondColor16, thirdColor16;
        firstColor16 = secondColor16 = thirdColor16 = rgb[0] << 8 | rgb[1];
        thirdColor = (thirdColor16 << (8 - thirdBitPP)) & 0x00FF;
        secondColor = ((secondColor16 >> thirdBitPP) << (8 - secondBitPP))  & 0x00FF;
        firstColor = ((firstColor16 >> (secondBitPP + thirdBitPP)) << (8 - firstBitPP))  & 0x00FF;
    }
    else {
        firstColor = secondColor = thirdColor = rgb[0];
        thirdColor = thirdColor << (8 - thirdBitPP);
        secondColor = (secondColor >> thirdBitPP) << (8 - secondBitPP);
        firstColor = (firstColor >> (secondBitPP + thirdBitPP)) << (8 - firstBitPP);
    }

    switch(colorSequence[0]) {
    case 'R':
        *r = firstColor;
        break;
    case 'G':
        *g = firstColor;
        break;
    case 'B':
        *b = firstColor;
        break;
    }
    switch(colorSequence[1]) {
    case 'R':
        *r = secondColor;
        break;
    case 'G':
        *g = secondColor;
        break;
    case 'B':
        *b = secondColor;
        break;
    }
    switch(colorSequence[2]) {
    case 'R':
        *r = thirdColor;
        break;
    case 'G':
        *g = thirdColor;
        break;
    case 'B':
        *b = thirdColor;
        break;
    }
}

void convert_color_buffer(guint8 *srcBuffer, guint8 *dstBuffer, int srcstart, int srcend, int dststart)
{
    guint8 r, g, b;
    int dstIdx = dststart;
    for(int i = srcstart; i < srcend; i += srcBPP) {
        convert_color(&r, &g, &b, srcBuffer + i);
        dstBuffer[dstIdx] = r;
        dstBuffer[dstIdx + 1] = g;
        dstBuffer[dstIdx + 2] = b;
        dstIdx += DSTBPP;
    }
}

void convert_color_buffer_all(guint8 *srcBuffer, guint8 *dstBuffer)
{
    convert_color_buffer(srcBuffer, dstBuffer, 0, srcBPP * Width * Height, 0);
}

gboolean load_frame (gpointer data)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    long long initialTime = t.tv_sec * 1000 + t.tv_usec / 1000;
    long elapsedTime;
    do {
        int n = 0;
        /*if(srcBPP * Width * Height - frameBytes >= (srcBPP * Width * Height)/8) */
        /*	n = fread (srcBuffer + frameBytes, 1, (srcBPP * Width * Height)/8, f); */
        /*else */
        n = fread (srcBuffer + frameBytes, 1, (srcBPP * Width * Height) - frameBytes, f);
        if(n > 0) {
            frameBytes += n;
            int traced = 0;
            if(showTrace && frameBytes <= (srcBPP * Width * (Height - 1))) {
                traced = 1;
                memset(srcBuffer + frameBytes, 255, srcBPP * Width - (frameBytes % (srcBPP * Width)));
            }

            if(srcBuffer != dstBuffer)
                convert_color_buffer(srcBuffer, dstBuffer, frameBytes - n, frameBytes + (traced == 1 ? srcBPP * Width - (frameBytes % (srcBPP * Width)) : 0), (frameBytes - n) / srcBPP * DSTBPP);
        }
        else if(errno != EAGAIN) {
            int errno_ = errno;
            g_print("Read return %d, errno: %d\n", n, errno_);
        }

        gettimeofday(&t, NULL);
        elapsedTime = (t.tv_sec * 1000 + t.tv_usec / 1000) - initialTime;
    }
    while(frameBytes < srcBPP * Width * Height && elapsedTime < msPerFrame);
    if(frameBytes == srcBPP * Width * Height) {
        if(srcBuffer != dstBuffer) {
            convert_color_buffer_all(srcBuffer, dstBuffer);
        }

        /*pixbuf = gdk_pixbuf_new_from_data (dstBuffer, GDK_COLORSPACE_RGB, HAS_ALPHA, 8, Width, Height, Width*DSTBPP, NULL, NULL); */
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
        frameBytes = 0;
	//char format[128] = "";
        //char maxbinarystr[512] = "";
	//sprintf(maxbinarystr, "%lld", 1LLL<<(sizeof(long long int)*8));
	//sprintf(format, "%%0%ldlld", strlen(maxbinarystr));
        //sprintf(filename, "~/testout/%s", format);
	//sprintf(filename, filename, currentFrame);
        if(saveScreenshot) {
            sprintf(fullpath, "%s/%032lld.jpg", basepath, currentFrame);
            gdk_pixbuf_save (pixbuf, fullpath, "jpeg", NULL, "quality", "100", NULL);
        }
        currentFrame++;
    }

    return G_SOURCE_CONTINUE;
}

static void refreshCallback( GtkWidget *widget,
                             gpointer data )
{
    /*pixbuf = gdk_pixbuf_new_from_data (dstBuffer, GDK_COLORSPACE_RGB, HAS_ALPHA, 8, Width, Height, Width*DSTBPP, NULL, NULL); */
    gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
}


int main (int argc, char *argv[])
{
    char fifoName[FIFO_NAME_SIZE + 1];
    int fps = 25;
    const struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"fifo", required_argument, 0, 'f'},
        {"width", required_argument, 0, 'x'},
        {"height", required_argument, 0, 'y'},
        {"rgbformat", optional_argument, 0, 'm'},
        {"fps", no_argument, 0, 'p'},
        {"showtrace", optional_argument, 0, 't'},
        {"screnshot", optional_argument, 0, 's'},
        {0, 0, 0, 0}
    };
    int option_index;

    progname = argv[0];

    progname = argv[0];

    if (argc < 2)
        usage();

    while(1) {
        int c;
        int bppSubparamL;
        char firstC, secondC, thirdC;
        c = getopt_long (argc, argv, ":hf:x:y:m:p:ts:",
                         long_options, &option_index);
        if (c < 0)
            break;

        switch (c) {
        case 'h':
            usage();
            break;
        case 'x':
            Width = atoi(optarg);
            if(Width <= 0)
                usage();

            break;
        case 'y':
            Height = atoi(optarg);
            if(Height <= 0)
                usage();

            break;
        case 'p':
            fps = atoi(optarg);
            if(fps <= 0)
                usage();

            if(fps < 5)
                fps = 5;
            else if(fps > 25)
                fps = 25;

            msPerFrame = 1000 / fps;
            break;
        case 'm':
            bppSubparamL = sscanf(optarg, "%c%c%c_%d_%d_%d", &firstC, &secondC, &thirdC, &firstBitPP, &secondBitPP, &thirdBitPP);

            if(bppSubparamL != 6)
                usage();

            if(!(firstC == 'R' || firstC == 'G' || firstC == 'B'))
                usage();

            if(!(secondC == 'R' || secondC == 'G' || secondC == 'B'))
                usage();

            if(!(thirdC == 'R' || thirdC == 'G' || thirdC == 'B'))
                usage();

            if(!(firstC != secondC && secondC != thirdC))
                usage();

            if(!(firstBitPP <= 8 && secondBitPP <= 8 && thirdBitPP <= 8))
                usage();

            if(!(firstBitPP >= 1 && secondBitPP >= 1 && thirdBitPP >= 1))
                usage();

            srcBPP = (firstBitPP + secondBitPP + thirdBitPP) / 8 + ((firstBitPP + secondBitPP + thirdBitPP) % 8 != 0 ? 1 : 0);
            colorSequence[0] = firstC;
            colorSequence[1] = secondC;
            colorSequence[2] = thirdC;
            colorSequence[3] = '\0';
            break;
        case 't':
            showTrace = 1;
            break;
        case 'f':
            strcpy(fifoName, optarg);
            break;
	case 's':
            basepath = malloc(strlen(optarg) + 1);
            fullpath = malloc(strlen(optarg) + 32 + 4 + 2);//4 extension, 2 path separator plus terminating 0
            strcpy(basepath, optarg);
            saveScreenshot = 1;
            break;
        case '?':
            fprintf(stderr, "Unexpected parameter %s\nAborting.\n", argv[optind - 1]);
            exit(ER_OPTIONS);
            break;
        case ':':
            fprintf(stderr, "Missing parameter for option %s\nAborting.\n", argv[optind - 1]);
            exit(ER_OPTIONS);
            break;
        default:
            usage();
            break;
        }
    }

    if (optind < argc)
        usage();

    //Instantiate buffer(s)
    dstBuffer = (guint8*)malloc(sizeof(guint8) * DSTBPP * Width * Height);
    if(strcmp(colorSequence, "RGB") != 0 || srcBPP != DSTBPP)
        srcBuffer = (guint8*)malloc(sizeof(guint8) * srcBPP * Width * Height);
    else
        srcBuffer = dstBuffer;

    //GTK stuff
    gtk_init (&argc, &argv);
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    /*gtk_window_set_default_size(GTK_WINDOW(window), Width+20, Height+20); */
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_title (GTK_WINDOW (window), "Virtual screen");
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit),
                      NULL);

    g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

    mainBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

    pixbuf = gdk_pixbuf_new_from_data (dstBuffer,
                                       GDK_COLORSPACE_RGB,
                                       HAS_ALPHA, 8, Width, Height, Width * DSTBPP, NULL, NULL);

    image = gtk_image_new_from_pixbuf (pixbuf);

    refreshBtn = gtk_button_new_from_icon_name ("view-refresh", GTK_ICON_SIZE_BUTTON);
    g_signal_connect(refreshBtn, "clicked", G_CALLBACK (refreshCallback), NULL);

    gtk_container_add(GTK_CONTAINER (window), mainBox);
    gtk_container_add(GTK_CONTAINER (mainBox), refreshBtn);
    gtk_container_add(GTK_CONTAINER (mainBox), image);

    gtk_window_set_accept_focus (GTK_WINDOW(window), TRUE);

    gtk_widget_show_all (window);

    //Execution info
    g_print("Frame width: %d\n"
            "Frame height: %d\n"
            "Bits Per Pixel read from file: %d\n"
            "Bits Per Pixel used: %d\n"
            "Sequence: %s (R:%d G:%d B:%d)\n"
            "FPS: %d (%d ms)\n"
            "Show trace: %s\n"
            "Screenshot: %s\n", Width, Height, srcBPP * 8, firstBitPP + secondBitPP + thirdBitPP, colorSequence, firstBitPP, secondBitPP, thirdBitPP, fps, msPerFrame, showTrace ? "true" : "false", saveScreenshot ? basepath : "NO");
    g_print("Will read %ld bytes per frame from %s\n", sizeof(guint8) * srcBPP * Width * Height, fifoName);

    //Fifo stuff
    /*g_print("Opening file %s\n", argv[1]); */
    /*char fifo_name[] = "~/vscreen_fifo_XXXXXX";
       mktemp(fifo_name);

       if (mkfifo(fifo_name,0666)<0) {
       g_error("Cannot create fifo %s", fifo_name);
       exit(1);
       }*/
    /*f = fopen (argv[1], "r"); */
    /*g_print("Opening file %s\n", fifo_name);
       if(fd = open(fifo_name, O_RDONLY) < 0) {
       g_error("Cannot open created fifo %s", fifo_name);
       exit(2);
       }
       g_print("File '%s' opened\n", fifo_name);
       //f = fdopen(fd, "r");
     */
    if((f = fopen(fifoName, "rb")) == NULL) {
        g_error("Cannot open fifo %s", fifoName);
        exit(ER_FIFO_OPEN);
    }

    /*fc = open(argv[2], O_RDONLY|O_NONBLOCK); */
    int flags = fcntl(fileno(f), F_GETFL, 0);
    fcntl(fileno(f), F_SETFL, flags | O_NONBLOCK);

    gint func_ref = g_timeout_add (msPerFrame, load_frame, NULL);/*40 ms = 25 fps */

    //Main loop
    gtk_main ();

    //Ending program, free resources
    g_source_remove(func_ref);
    free (dstBuffer);
    if(srcBPP != DSTBPP)
        free (srcBuffer);

    if(saveScreenshot) {
        free(basepath);
        free(fullpath);
    }

    fclose (f);
    /*close(fc); */
    /*close(fd); */

    return ER_OK;
}
