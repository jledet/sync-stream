#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <gst/gst.h>
#include <gst/net/gstnet.h>

#define DATA_GROUP	"224.5.0.100"
#define DATA_PORT	16990
#define TIME_HOST	"127.0.0.1"
#define TIME_PORT	16991
#define ANNC_GROUP	"224.5.0.101"
#define ANNC_PORT	16992

GstElement *pipeline;
GstElement *src, *buffer, *depay, *decode, *sink;

GstClock* clk;

static uint64_t get_basetime(void)
{
	struct sockaddr_in addr;
	socklen_t addrlen;
	int sock, cnt;
	struct ip_mreq mreq;
	unsigned int opt = 1;
	uint64_t basetime;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	assert(sock >= 0);

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		perror("Reusing ADDR failed");
		exit(1);
	}

	memset((char *)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(ANNC_PORT);
	addrlen = sizeof(addr);

	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		exit(1);
	}

	mreq.imr_multiaddr.s_addr = inet_addr(ANNC_GROUP);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		perror("setsockopt mreq");
		exit(1);
	}

	cnt = recvfrom(sock, &basetime, sizeof(basetime), 0, (struct sockaddr *) &addr, &addrlen);
	assert(cnt == sizeof(basetime));

	return be64toh(basetime);
}

void setup_network_time(void)
{
	uint64_t basetime;

	printf("Waiting for server basetime... ");
	fflush(stdout);

	basetime = get_basetime();

	printf("%lu\n", basetime);

	clk = gst_net_client_clock_new("clock", TIME_HOST, TIME_PORT, basetime);
        gst_element_set_start_time(GST_ELEMENT(pipeline), GST_CLOCK_TIME_NONE);
        gst_pipeline_use_clock(GST_PIPELINE(pipeline), clk);
        gst_element_set_base_time(GST_ELEMENT(pipeline), basetime);
}

int main(int argc, char **argv)
{
	bool quit = false;
	GstMessage* msg;

	gst_init(NULL, NULL);

	/* Create elements */
	src    = gst_element_factory_make("udpsrc", "src");
	buffer = gst_element_factory_make("rtpjitterbuffer", "buffer");
	depay  = gst_element_factory_make("rtph264depay", "depay");
	decode = gst_element_factory_make("avdec_h264", "decode");
	sink   = gst_element_factory_make("autovideosink", "sink");

	/* Setup elements */
	g_object_set(src, "multicast-group", DATA_GROUP, NULL);
	g_object_set(src, "auto-multicast", true, NULL);

	g_object_set(src, "caps",
		     gst_caps_new_simple(
			"application/x-rtp",
			"media",      G_TYPE_STRING, "video",
			"clock-rate", G_TYPE_INT,    90000,
			"payload",    G_TYPE_INT,    96,
			"encoding-name", G_TYPE_STRING, "H264",
			NULL), NULL);

	g_object_set(buffer, "latency", 1000, NULL);

	/* Build pipelines */
	pipeline = gst_pipeline_new("test");

	gst_bin_add_many(GST_BIN(pipeline), src, buffer, depay, decode, sink, NULL);

	gst_element_link_many(src, buffer, depay, decode, sink, NULL);

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
