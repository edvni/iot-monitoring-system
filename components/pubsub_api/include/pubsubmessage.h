/*
    This file is for formatting the pubsub JSON message

    The format of Pub/Sub message is defined here:
    https://cloud.google.com/pubsub/docs/reference/rest/v1/PubsubMessage
*/

// Pub/Sub message JSON format

const int message_buffer_length = 128;
const int all_messages_buffer_length = 1024;

const char* message_format = "%s{"
                            
                            "\"data\": \"%s\","
                            "\"ordering_key\": \"first order\","
                            "\"attributes\":{"
                            "\"temperature\": \"%f\","         
                            "\"humidity\": \"%f\"" 
                            "}}";

const char* log_format = "%lld,%s,%f,%f\n";