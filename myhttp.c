#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>


int get(int sockfd, char* resource);
int trace(int sockfd, char* resource);
int head(int sockfd, char* resource);

/**
 * @brief      Set up socket for client  based on the Address family INET
 *
 * @param[in]  argc - The counter in which the arguments from command line are stories in
 *             argv - The arguments that are written into command line
 */
int main(int argc, char *argv[])
{
   int sockfd, portno, n;
   // Strucct to hold the socket adress and the server address
   struct sockaddr_in serv_addr;
   // Struct holding the hostname of the server
   struct hostent *server;

   char buffer[512];

   fprintf(stderr, "\nUse format ./myhttp hostname portnumber HTTPmethod /filename.filetype \n");
   fprintf(stderr, "Example format ./myhttp localhost 5001 get /index.html \n\n\n");

   if (argc < 3)
   {
      fprintf(stderr, "%s Is a Incorrect Hostname or Port\n", argv[0]);
      exit(0);
   }
   // save port number from argv2
   portno = atoi(argv[2]);


   // Socket point saved into sockfd
   sockfd = socket(AF_INET, SOCK_STREAM, 0);

   // if sockfd returns value less then 0 error will be written
   if (sockfd < 0)
   {
      perror("Error - Cannot open socket");
      exit(1);
   }

   // Save hostname into server
   server = gethostbyname(argv[1]);

   // If hostname is null then print error cannot connect
   if (server == NULL)
   {
      fprintf(stderr, "Error - No hostname found\n");
      exit(0);
   }

   // Set server address to zero
   bzero((char *) &serv_addr, sizeof(serv_addr));
   // Assign server address using family Address family internet
   serv_addr.sin_family = AF_INET;
   // Send copy of server address to the sever address code
   bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
   // Assign server address to server port number
   serv_addr.sin_port = htons(portno);

   // Client can connect to server
   if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
   {
      perror("Error - Cannot connect");
      exit(1);
   }


   // check if GET method is called based on string compare
   if (strcmp (argv[3], "get") == 0)
   {
      // Sockfd calls get method sending in the file needed
      sockfd = get(sockfd, argv[4]);
   }

   // check if TRACE method is called based on string compare
   else if (strcmp (argv[3], "trace") == 0)
   {
      // Sockfd calls trace method sending in the file needed
      sockfd = trace(sockfd, argv[4]);
   }

   // check if HEAD method is called based on string compare
   else if (strcmp (argv[3], "head") == 0)
   {
      // Sockfd calls head method sending in the file needed
      sockfd = head(sockfd, argv[4]);
   }

   //Check if n is less than 0
   if (n < 0)
   {
      perror("Error - Cannot write to socket");
      exit(1);
   }

   // Set buffer to 0
   bzero(buffer, 512);
   // Read response from server
   n = read(sockfd, buffer, 512);

   if (n < 0)
   {
      perror("Error - Cannot read from socket");
      exit(1);
   }

   printf("%s\n", buffer);
   return 0;
}


/**
 * @brief      Client GET method creates request header
 *
 * @param[in]  sockfd - Use to write the header to socket file descriptor
 *             resource - File that is requested by the client
 */
int get (int sockfd , char* resource)
{
   // Create correct size to hold the whole request header
   int size = 12 + strlen (resource) + 1;

   char *header;
   printf("Size of resource is:%d\n\n", size );
   // Assign memory location the size of the whole header
   header = malloc(size);
   // Assign first index to \0
   header[0] = '\0';

   // Put GET at start of header
   strcat (header, "GET ");
   // Concatenate the resource onto header
   strcat (header, resource);
   // Concatenate HTTP/1.1 onto the end of resource
   strcat (header, " HTTP/1.1");
   // Send request header to server
   int x = write(sockfd, header , strlen(header));

   return (sockfd);

}

/**
 * @brief      Client HEAD method creates request header
 *
 * @param[in]  sockfd - Use to write the header to socket file descriptor
 *             resource - File that is requested by the client
 */
int head (int sockfd , char* resource)
{
   // Create correct size to hold the whole request header
   int size = 13 + strlen (resource) + 1;

   char *header;
   // Assign memory location the size of the whole header
   header = malloc(size);
   // Assign first index to \0
   header[0] = '\0';

   // Put HEAD at start of header
   strcat (header, "HEAD ");
   // Concatenate the resource onto header
   strcat (header, resource);
   // Concatenate HTTP/1.1 onto the end of resource
   strcat (header, " HTTP/1.1");

   // Send request header to server
   int x = write(sockfd, header , strlen(header));

   return (sockfd);

}

/**
 * @brief      Client TRACE method creates request header
 *
 * @param[in]  sockfd - Use to write the header to socket file descriptor
 *             resource - File that is requested by the client
 */
int trace (int sockfd, char* resource)
{
   // Create correct size to hold the whole request header
   int size = 14 + strlen (resource) + 1;

   char *header;
   // Assign memory location the size of the whole header
   header = malloc(size);
   // Assign first index to \0
   header[0] = '\0';

   // Put TRACE at start of header
   strcat (header, "TRACE ");
   // Concatenate the resource onto header
   strcat (header, resource);
   // Concatenate HTTP/1.1 onto the end of resource
   strcat (header, " HTTP/1.1");

   // Send request header to server
   int x = write(sockfd, header , strlen(header));

   return (sockfd);

}

