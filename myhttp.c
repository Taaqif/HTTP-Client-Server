#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>

#define BUFF_SIZE 512
#define DEFAULT_PORT 80

int get(int sockfd, char *resource);
int trace(int sockfd, char *resource);
int head(int sockfd, char *resource);

int contentOnly = 1;
/**
 * @brief      Set up socket for client  based on the Address family INET
 *
 * @param[in]  argc - The counter in which the arguments from command line are stories in
 *             argv - The arguments that are written into command line
 */
int main(int argc, char *argv[])
{
    int sockfd, n;
    int portno = DEFAULT_PORT;
    // Strucct to hold the socket adress and the server address
    struct sockaddr_in serv_addr;
    // Struct holding the hostname of the server
    struct hostent *server;

    char buffer[BUFF_SIZE];
    int opt, index;
    int numArgs = 0;
    int succ_parsing = 0;
    char *url;
    char *method = "GET";

    while ((opt = getopt(argc, argv, "m:a")) != -1)
    {
        switch (opt)
        {
        case 'm':
            method = optarg;
            break;
        case 'a':
            contentOnly = 0;
            break;
        default:
            fprintf(stderr, "Usage: \n%s \t[ -m <method> ] Method to send\n\
               \t[ -a ] View response content only\n\
               \t< url >\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    for (index = optind; index < argc; index++)
    {
        if (numArgs > 0)
        {
            fprintf(stderr, "Incorrect usage. Only supports one URL");
            exit(EXIT_FAILURE);
        }
        numArgs++;

        url = argv[index];
    }

    //parse url
    //copied from lines 53-56
    //https://github.com/luismartingil/scripts/blob/master/c_parse_http_url/parse_http_uri.c
    char ip[200];
    int port = 80;
    char page[200] = "";
    if (strstr(url, "http://") != NULL)
    {
        url += 7;
    }
    if (strstr(url, "https://") != NULL)
    {
        url += 8;
    }
    if (sscanf(url, "%99[^:]:%i/%199[^\n]", ip, &port, page) == 3)
    {
        succ_parsing = 1;
    }
    else if (sscanf(url, "%99[^/]/%199[^\n]", ip, page) == 2)
    {
        succ_parsing = 1;
    }
    else if (sscanf(url, "%99[^:]:%i[^\n]", ip, &port) == 2)
    {
        succ_parsing = 1;
    }
    else if (sscanf(url, "%99[^\n]", ip) == 1)
    {
        succ_parsing = 1;
    }

    if (!succ_parsing)
    {
        fprintf(stderr, "Error parsing url. Try host.com:port/resource\n");
        exit(1);
    }

    portno = port;

    // if sockfd returns value less then 0 error will be written
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Error - Cannot open socket");
        exit(1);
    }

    // Save hostname into server
    // If hostname is null then print error cannot connect
    if ((server = gethostbyname(ip)) == NULL)
    {
        fprintf(stderr, "Error - No hostname found\n");
        exit(0);
    }

    // Set server address to zero
    bzero((char *)&serv_addr, sizeof(serv_addr));
    // Assign server address using family Address family internet
    serv_addr.sin_family = AF_INET;
    // Send copy of server address to the sever address code
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    // Assign server address to server port number
    serv_addr.sin_port = htons(portno);

    // Client can connect to server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Error - Cannot connect");
        exit(1);
    }
    //    fprintf(stdout, "Get request%s", method);
    //    // check if GET method is called based on string compare

    if (strcasecmp(method, "get") == 0 | strcasecmp(method, "head") == 0 | strcasecmp(method, "trace") == 0)
    {
        bzero(buffer, BUFF_SIZE);

        //display message for -m HEAD
        if(strcasecmp(method, "head") == 0 && contentOnly){
            fprintf(stderr, "NOTE: Head method does not return any content. Use -a to see response\n");
        }
        sprintf(buffer, "%s /%s HTTP/1.1\nHost: %s:%d\n\n", method, page, ip, port);
        if ((write(sockfd, buffer, strlen(buffer))) < 0)
        {
            perror("Error - Cannot write to socket");
            exit(1);
        }
        fprintf(stdout, "%s", buffer);
    }
    else
    {
        fprintf(stderr, "Method %s not supported. Try HEAD, GET, TRACE\n", method);
        exit(1);
    }

    //this is where you will either print the body or the whole message
    int headFound = 0;
    // Read response from server
    while (1)
    {
        // char * lineToken;
        bzero(buffer, BUFF_SIZE);
        //accomodate null character
        n = read(sockfd, buffer, BUFF_SIZE - 1);
        // fprintf(stderr, "%s", buffer);
        char *lineToken = malloc(strlen(buffer) + 1);
        strcpy(lineToken, buffer);

        while ((lineToken = strtok(lineToken, "\n")) != NULL)
        {
            
            //check if the user only wants the content
            if(!contentOnly)
            {
                fprintf(stdout, "%s\n", lineToken);
                
            }else{
                //check if the header seperateor has been found and print 
                if(headFound)
                {
                    fprintf(stdout, "%s\n", lineToken);
                }
            }
            //check if new line character 
            if(strlen(lineToken) == 1)
            {
                headFound = 1;
                //skip the next two lines
                strtok(NULL, "\n");
            }
            //just to style the response correctly
            
            lineToken = NULL;

        }
        if(n != BUFF_SIZE-1)
        {
            fprintf(stdout, "\n");
            
        }
        //might be an issue when the response is exactly 512 bytes long
        
        free(lineToken);
        if(n + 1 != BUFF_SIZE)
        {
            break;
        }
        if (n <= 0 || n == -1)
            break;
    }

    return 0;
}
