// PubSub JWT configuration


// PubSub
#define TOPIC_NAME "ruuvitag_inno_kevat2025"

#define PUBSUB_URL "https://pubsub.googleapis.com/v1/projects/prj-mtp-jaak-leht-ufl/topics/ruuvitag_inno_kevat2025:publish" // PubSub URL for publishing messages

//Create a copy of this file and change the name to config.h
//change the fields to your needs. 
//You must have created the service account for google cloud in order to fill the jwt and pubsub parts. 


// JWT
//Private key of the service account. This must be acquired from google cloud. One service account can have multiple keyfiles. 
//The steps to acquire this is explained in the setup part of github wiki.
//They keyfile is json file that contains information about the account aswell as the private key required for authentication to Oauth2. The keyfile by its self is not enought to publish to pubsub.
#define PRIVATE_KEY "-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQC5pQUqhxJSEs2V\nPaO78jghJS1W7ZDJd17eUUkDBCP0p8RaLSw/oySLHQQ8jE1nFXyA1E0xTw8hZL3O\n/BnB8u24xsmuJt3kqllPC/Xdd2Em/j+smc7zfZGvYpvUcrzjaw7xO3iSxqlti15j\neXU3CoFJPenbp50fhTH8SdcB1X6npXPenNgOt9c1qRzA36a08bZ5l4s4PdCemj2b\nXy/urpT5+2jNvFe62wa9/sS/R38bgmpoFla+lwci4QyZyUdPdvhZuz329HWYx15f\ndiH77TNOug9NBa348TJ23Sh2fkDHlImJiwXtqxfVHPGktlI62jb6e2QDd+pbjTE2\nO/tHXRtDAgMBAAECggEACuE7IHHCZvFM1MfELkd09wBSRsDCgQQlblsOL9vmGDhd\nf2QcA+4Vk1o+fXvtggfrgDNbrh2c3I9RDm3jkTnMU81r/sWusHA0BA1xOdWoFHHH\nHTWgatBmTC83xGd7CXWkmQpWt9IRFjyaP13cED87MiXWQaDHn77Ic1QygnGBgY/3\ncx7F2b7UahluEJaPPCNBtwbXYUNWPPdyaJWitsCiorh0DwElXeFPz42Ey9YyjLFN\nugMPmtwWgj21X/ilqaltFo9jx1toZsLb3yweLkUgPnS1Bn9iaVw9ebxP5pfLI2MW\n372z2R4K1eT7gdHZJiJFB9MIIBY6gfUfnEU8IlSYQQKBgQD7m9PAxVcDDCSy47G5\nzMwtE8qkHn9SPhK2Z7QdTAuP+qS8hLC57WFDVbnFgeffFY0oneB2oidQxKomTqKS\nHOaOjSbeAFiPlZ+3RwEeTpfhxCkzGBFSW0SKEzCbYBL9OzPpetSg211Av/8wLr2w\n/Q7eZBXwUCHmpxq8yJlWgVcBwQKBgQC84ngnqN4ipl3/89JeQgnuKCpjf/w5gU9Z\nQQ/5tv98FMiV0LPJ26ZJDdKe7QN8aoYWJR2x8tf57nJKQoeFtZ5fEa1bov4SMQQ3\n3K6OXZXv2I1cuIsH/tq22sBQxg6L3XSuZiTakbHgjSK6VMQN8PMjNwvRt3eRS4n0\nzralSpGWAwKBgC7KwAqtqIzsiTarmbXQHHiVoa377fnBiYFar+hy6AOSvWhB1Qv+\n1YPMQYw1qIWYYHQZSFFHvsAKkwokvZ8muMnx/jRzJAUv8lAHaHHWc/CMpozWAQKr\n7ocvIm8C4wUtKW5WZYy0vxb3neG8D5MGvOkm+92BSYy3rwVE2R95VlEBAoGBAKGU\nXlMW+0fK5/ivnFMzzQjlZTWO47ZRrw5cQQ1OhYmgweKfEV3pv3sZDwcTG/tPqPrC\nr39tjWJpn211MjeooR0UoycT7KKlOcWLy+tPlZcCuhMKbyzc0D7CLOgsNX9iZ5FV\nIcuz/OVp9r5NCxYd+/AxA7RYbTlm1FIOj2Igwx//AoGAMbFrQFf545vQn65ICZ+w\ncN86eoFRq+P8Ktf+X/WLDLxHJQ6mQIkeUpu6W5mDdmd7H/p3QKf2t9SOTEhjmMRd\n++HQQ5UXUKnTdSMthiprD4vCENzFWiJ40Msxxn/uswRgTwmgYI3n4QQRVTie/ioY\nHrdRloLXGNcPmij9sHkDRm0=\n-----END PRIVATE KEY-----\n"
#define KEY_ID      "24eed1ffb5ec9ac326d49274973b3657bd329a83"   //Header "kid"
#define ISS         "viherpysakki@prj-mtp-jaak-leht-ufl.iam.gserviceaccount.com" //Payload "iss"
#define SUB         "viherpysakki@prj-mtp-jaak-leht-ufl.iam.gserviceaccount.com" //Payload "sub"

#define TOKEN_EXPIRATION_TIME_IN_SECONDS 3600   // Token expiration time in seconds (can be configured to match sensor reading interval)