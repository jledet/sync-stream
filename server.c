#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <gst/gst.h>
#include <gst/gstcaps.h>
#include <gst/net/gstnet.h>

#define DATA_GROUP	"224.5.0.100"
#define DATA_PORT	16990
#define TIME_HOST	"127.0.0.1"
#define TIME_PORT	16991
#define ANNC_GROUP	"224.5.0.101"
#define ANNC_PORT	16992

GstElement *pipeline;
GstElement *src, *filt, *rate, *filt2, *enc, *pay, *sink;

GstNetTimeProvider *timeprovider;
pthread_t time_thread;
uint64_t basetime;

static void *announce_thread(void *param)
{
	uint64_t basetime_tx;
	struct sockaddr_in addr;
	int addrlen, sock, cnt;

	basetime_tx = htobe64(basetime);

	/* set up socket */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}

	unsigned int val = 1;
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
		perror("Reusing ADDR failed");
		exit(1);
	}

	memset((char *)&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(ANNC_PORT);
	addrlen = sizeof(addr);

	addr.sin_addr.s_addr = inet_addr(ANNC_GROUP);
	while (1) {
		cnt = sendto(sock, &basetime_tx, sizeof(basetime_tx),
			     0, (struct sockaddr *) &addr, addrlen);
		assert(cnt == sizeof(basetime_tx));
		sleep(1);
	}
}

static void setup_network_time(void)
{
	int ret;

	GstClock* clk = gst_system_clock_obtain();
        assert(clk);

        gst_pipeline_use_clock(GST_PIPELINE(pipeline), clk);
        basetime = gst_clock_get_time(clk);

        timeprovider = gst_net_time_provider_new(clk, NULL, TIME_PORT);
        assert(timeprovider);

        gst_element_set_start_time(GST_ELEMENT(pipeline), GST_CLOCK_TIME_NONE);
        gst_element_set_base_time(GST_ELEMENT(pipeline), basetime);

        g_print("Basetime %lu\n", basetime);

        // Create basetime reply server
	ret = pthread_create(&time_thread, NULL, announce_thread, NULL);
	assert(ret == 0);
}

int main(int argc, char **argv)
{
	bool quit = false;
	GstMessage* msg;

	gst_init(NULL, NULL);

	/* Create elements */
	src   = gst_element_factory_make("videotestsrc", NULL);
	filt  = gst_element_factory_make("capsfilter", NULL);
	rate  = gst_element_factory_make("videorate", NULL);
	filt2 = gst_element_factory_make("capsfilter", NULL);
	enc   = gst_element_factory_make("x264enc", NULL);
	pay   = gst_element_factory_make("rtph264pay", NULL);
	sink  = gst_element_factory_make("udpsink", NULL);

	/* Setup elements */
	g_object_set(src, "pattern", 12, NULL);

	g_object_set(filt, "caps", gst_caps_from_string("video/x-raw,width=640,height=480,framerate=1/1"), NULL);

	g_object_set(filt2, "caps", gst_caps_from_string("video/x-raw,framerate=25/1"), NULL);

	g_object_set(enc, "bitrate", 1024, "key-int-max", 24, NULL);

	g_object_set(pay, "pt", 96, NULL);
	g_object_set(pay, "config-interval", 1, NULL);

	g_object_set(sink, "host", DATA_GROUP, NULL);
	g_object_set(sink, "auto-multicast", true, NULL);

	/* Build pipelines */
	pipeline = gst_pipeline_new("test");

	gst_bin_add_many(GST_BIN(pipeline), src, filt, rate, filt2, enc, pay, sink, NULL);

	gst_element_link_many(src, filt, rate, filt2, enc, pay, sink, NULL);

	/* Use network time */
	setup_network_time();
		
	/* Start playback */
	gst_element_set_state(pipeline, GST_STATE_PLAYING);

	GstBus* bus = gst_element_get_bus(pipeline);

	while (!quit) {
		msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

		GError *err;
		gchar *debug_info;

		switch (GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_ERROR: {
			gst_message_parse_error(msg, &err, &debug_info);
			printf("ERROR: %s\n", debug_info);
			quit = true;
			break;
		}
		case GST_MESSAGE_EOS:
			printf("Finished\n");
			quit = true;
			break;
		default:
			printf("Unknown\n");
			quit = true;
			break;
		}
	}

	gst_element_set_state(pipeline, GST_STATE_NULL);

	return 0;
}
