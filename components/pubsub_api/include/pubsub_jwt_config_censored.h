// PubSub JWT configuration


// PubSub
#define PROJECT_ID "prj-mtp-jaak-leht-ufl"
#define TOPIC_NAME "ruuvitag_inno_kevat2025"

//Create a copy of this file and change the name to config.h
//change the fields to your needs. 
//You must have created the service account for google cloud in order to fill the jwt and pubsub parts. 


// JWT
//Private key of the service account. This must be acquired from google cloud. One service account can have multiple keyfiles. 
//The steps to acquire this is explained in the setup part of github wiki.
//They keyfile is json file that contains information about the account aswell as the private key required for authentication to Oauth2. The keyfile by its self is not enought to publish to pubsub.
#define PRIVATE_KEY "-----BEGIN PRIVATE KEY-----\nMIxxxx\n-----END PRIVATE KEY-----\n" // censored service account key
#define KEY_ID      "123456789abcdefghijklmnopqrstuvwxyzo9999"   //Header "kid" (censored)
#define ISS         "topicname@projectname.iam.gserviceaccount.com" //Payload "iss"
#define SUB         "topicname@projectname.iam.gserviceaccount.com" //Payload "sub"

#define TOKEN_EXPIRATION_TIME_IN_SECONDS 3600   // Token expiration time in seconds (can be configured to match sensor reading interval)