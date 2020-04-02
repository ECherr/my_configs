/***********************************************
#
#      Filename: final.c
#
#        Author: arvin 
#   Description: ---
#        Create: 2020-04-01 23:29:12
# Last Modified: 2020-04-01 23:29:12
***********************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <gst/gst.h>
#include <string.h>

#define RECEIVE_OWN_VIDEO_CAPS "application/x-rtp, encoding-name=JPEG,payload=26"
#define DEVICE                      "/dev/video0"
#define CLIENTS                     "192.168.199.217:5003,192.168.199.105:5003"

static const struct option long_options[] = {
    {"help", 1, NULL, 'h'},
    {"port", 1, NULL, 'p'},
    {"clients", 1, NULL, 'c'},
    {"device", 1, NULL, 'd'}
};

void usage(){
    printf("Usage:\n");
    printf("\t-h\t\t\tprintf this message.\n");
    printf("\t-p\t\t\tset the video-receive port\n");
    printf("\t-c\t\t\tset the video-send clients\n");
}

int main(int argc, char *argv[]){
    char *video_receive_port = (char *)malloc(sizeof(optarg));
    char *video_send_clients = (char *)malloc(sizeof(optarg));
    char *device = (char *)malloc(sizeof(optarg));
    char *multiClients = (char *)malloc(sizeof(optarg));
    video_receive_port = "5003";
    device = "/dev/video0";
    // multiClients = "192.168.199.105:5003,192.168.199.217:5003";
    int port;
    int opt = 0;
    
    while((opt = getopt_long(argc, argv, "hp:c:d:", long_options, NULL)) != -1){
        switch(opt){
            case 'h':  
                usage();
                break;
            case 'p':
                printf("you're setting video-receive port to %s\n", optarg);
                strcpy(video_receive_port, optarg);
                break;
            case 'c':
                strcpy(video_send_clients, optarg);
                // *video_send_clients = *optarg;
                // printf("ahhh,seaeroororr\n");
                break;
            case 'd': 
                strcpy(device, optarg);
                break;
            case '?':
                usage(); 
                break;
            case ':':
                usage();
                break;
            default:
                usage();
                break;
        }
    }

    printf("port \t%s\nclients \t%s\n", video_receive_port, video_send_clients);

    //先测试两个线程，一个发送，一个自己接收
    //receive
    /*
    gst-launch-1.0 -e -v udpsrc port=5003 ! 
    application/x-rtp, encoding-name=JPEG,payload=26 
    ! rtpjpegdepay ! jpegparse ! jpegdec ! autovideosink
    */
    GstElement *receive_pipeline, *send_pipeline;
    //receive_pipeline's element
    GstElement *udpsrc, *rtpjpegdepay, *jpegparse, *jpegdec, *autovideosink, *v4l2src, *rtpjpegpay, *multiudpsink;
    //send_pipeline's element
    GstCaps *rec_caps;
    GstBus *rec_bus, *send_bus;
    GstMessage *rec_msg, *send_msg;
    GstStateChangeReturn rec_ret, send_ret;

    //Initialize GStreamer
    gst_init(&argc, &argv);

    /*
     *create rec-video pipeline
     */

    receive_pipeline = gst_pipeline_new(NULL);

    //create elements
    udpsrc = gst_element_factory_make("udpsrc", "src");
    rtpjpegdepay = gst_element_factory_make("rtpjpegdepay", "depay");
    jpegparse = gst_element_factory_make("jpegparse", "parse");
    jpegdec = gst_element_factory_make("jpegdec", "dec");
    autovideosink = gst_element_factory_make("autovideosink", "sink");

    if(!receive_pipeline || !udpsrc || !rtpjpegdepay || ! jpegparse || !jpegdec || !autovideosink){
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    /*setting the rec-video pipeline
     * and link the elements
    */

    g_object_set(udpsrc,"port", atoi(video_receive_port), NULL);
    rec_caps = gst_caps_from_string(RECEIVE_OWN_VIDEO_CAPS);
    g_object_set(udpsrc, "caps", rec_caps, NULL);
    gst_caps_unref(rec_caps);
    
    gst_bin_add_many(GST_BIN(receive_pipeline), udpsrc, rtpjpegdepay, jpegparse, jpegdec, autovideosink, NULL);
    if(gst_element_link_many(udpsrc, rtpjpegdepay, jpegparse, jpegdec, autovideosink, NULL) != TRUE){
        g_printerr("Elements could not be linked.\n");
        return -1;
    }

    /*
     *create video send pipeline!
     */
    send_pipeline = gst_pipeline_new(NULL);

    v4l2src = gst_element_factory_make("v4l2src", "src");
    rtpjpegpay = gst_element_factory_make("rtpjpegpay", "pay");
    multiudpsink = gst_element_factory_make("multiudpsink", "sink");

    if(!send_pipeline || !v4l2src || !rtpjpegpay || !multiudpsink){
        g_printerr("Not all elements could be created.\n");
        return -1;
    }
    
    /*set the video-send pipeline and link the elements*/
    g_object_set(v4l2src, "device", device, NULL);
    g_object_set(multiudpsink, "clients", video_send_clients, NULL);

    gst_bin_add_many(GST_BIN(send_pipeline), v4l2src, rtpjpegpay, multiudpsink, NULL);
    if(gst_element_link_many(v4l2src, rtpjpegpay, multiudpsink, NULL) != TRUE){
        g_printerr("Elements could not be linked.\n");
        return -1;
    }
    
    /* setting state for send pipeline*/
    send_ret = gst_element_set_state(send_pipeline, GST_STATE_PLAYING);
    if(send_ret == GST_STATE_CHANGE_FAILURE){
        g_printerr("Unable to set the receivePipeline to the playing state.\n");
        return -1;
    }

    /* seting state for rec-pipeline*/
    rec_ret = gst_element_set_state(receive_pipeline, GST_STATE_PLAYING);
    if(rec_ret == GST_STATE_CHANGE_FAILURE){
        g_printerr("Unable to set the receivePipeline to the playing state.\n");
        return -1;
    }


    rec_bus = gst_element_get_bus(receive_pipeline);
    rec_msg = gst_bus_timed_pop_filtered(rec_bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    
    send_bus = gst_element_get_bus(send_pipeline);
    send_msg = gst_bus_timed_pop_filtered(send_bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);


    if(rec_msg != NULL){
        GError *err;
        gchar *debug_info;
        switch (GST_MESSAGE_TYPE(rec_msg))
        {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(rec_msg, &err, &debug_info);
                g_printerr("Error received from element %s: %s\n",
                    GST_OBJECT_NAME(rec_msg->src), err->message);
                g_printerr("Debugging information: %s\n",
                    debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_free(debug_info);
                break;
            case GST_MESSAGE_EOS:
                g_print("End-Of-Stream reached.\n");
                break;
            default:
                /* We should not reach here because we only aked for ERRORs and EOS */
                g_printerr("Unexpected message received.\n");
                break;
        }
        gst_message_unref(rec_msg);
    }

    if(send_msg != NULL){
        GError *err;
        gchar *debug_info;
        switch (GST_MESSAGE_TYPE(send_msg))
        {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(send_msg, &err, &debug_info);
                g_printerr("Error received from element %s: %s\n",
                    GST_OBJECT_NAME(send_msg->src), err->message);
                g_printerr("Debugging information: %s\n",
                    debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_free(debug_info);
                break;
            case GST_MESSAGE_EOS:
                g_print("End-Of-Stream reached.\n");
                break;
            default:
                /* We should not reach here because we only aked for ERRORs and EOS */
                g_printerr("Unexpected message received.\n");
                break;
        }
        gst_message_unref(send_msg);
    }

    gst_object_unref(rec_bus);
    gst_element_set_state(receive_pipeline, GST_STATE_NULL);
    gst_object_unref(receive_pipeline);

    gst_object_unref(send_bus);
    gst_element_set_state(send_pipeline, GST_STATE_NULL);
    gst_object_unref(send_pipeline);   
    return 0;
}
