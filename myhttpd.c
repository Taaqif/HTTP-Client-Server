#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>

#define BUFF_SIZE 512
#define MAX_CLIENTS 10

#define DEFAULT_PORT 8000
#define DEFAULT_ROOT_DIR "."
#define DEFAULT_LOG_FILE "httpd.log"

void processrequest(int sock);

void request(int sock, char *resource, char *host, int headOnly);
void serve400(int sock, int headOnly);
void serve404(int sock, int headOnly);

void trace(int sock, char *resource, char *host, char *echo);

void writelog(char *method, char *host, char *resource, int status);

void catch (int signo);
void claim_zombie();

int daemon_init(void);

FILE *logfile;

// struct
// {
//     char *ext;
//     char *filetype;
// } extensions[];

/**
 * @brief      Set up socket for server to bind to and then start listening, when client has connected process will be called
 *
 * @param[in]  argc - The counter in which the arguments from command line are stories in
 *             argv - The arguments that are written into command line
 */
int main(int argc, char *argv[])
{
    // extensions = {
    //     {"gif", "image/gif"},
    //     {"jpg", "image/jpeg"},
    //     {"jpeg", "image/jpeg"},
    //     {"png", "image/png"},
    //     {"zip", "image/zip"},
    //     {"gz", "image/gz"},
    //     {"tar", "image/tar"},
    //     {"htm", "text/html"},
    //     {"html", "text/html"},
    //     {"php", "image/php"},
    //     {"cgi", "text/cgi"},
    //     {"asp", "text/asp"},
    //     {"jsp", "image/jsp"},
    //     {"xml", "text/xml"},
    //     {"js", "text/js"},
    //     {"css", "test/css"}};
    int portno = DEFAULT_PORT;
    char *logfilename = DEFAULT_LOG_FILE;
    char *rootdir = DEFAULT_ROOT_DIR;
    int opt;

    while ((opt = getopt(argc, argv, "p:d:l:m:f:")) != -1)
    {
        switch (opt)
        {
        case 'p':
            portno = atoi(optarg);
            break;
        case 'd':
            if (chdir(optarg) == -1)
            {
                fprintf(stderr, "ERROR: Can't Change to directory %s\n", optarg);
                exit(0);
            }
            break;
        case 'l':
            logfilename = optarg;
            printf("%sl", optarg);
            break;
        case 'm':
            printf("%sm", optarg);
            break;
        case 'f':
            printf("%sf", optarg);
            break;
        default:
            fprintf(stderr, "Usage: \n%s \t[ -p <port number> ]\n\
            \t[ -d <document root> ]\n\
            \t[ -l <log file> ]\n\
            \t[ -m <file for mime types> ]\n\
            \t[ -f <number of preforks> ]",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    //Open logfile file for writing
    logfile = fopen(logfilename, "a+");

    //check valid port no
    if (portno < 0 || portno > 65535)
    {
        fprintf(stderr, "Invalid port entered. Defaulting to port: %d\n", DEFAULT_PORT);
        portno = DEFAULT_PORT;
    }
    //TODO: check valid dir

    //daemon_init();
    int sockfd, newsockfd, clilen;
    // Buffer that is allocated a size of 256
    char buffer[BUFF_SIZE];
    // Struct that holds a socket address, server address and client address
    struct sockaddr_in serv_addr, cli_addr;
    // Int holding variable and a process ID
    int n, pid;

    // Declaring the signal handler for handling zombie and control c
    struct sigaction act1, zact;
    act1.sa_flags = 0;
    // Handler for control C
    act1.sa_handler = catch;
    // Handler for claiming zombies
    zact.sa_handler = claim_zombie;
    sigemptyset(&zact.sa_mask);
    zact.sa_flags = SA_NOCLDSTOP;
    //sigaction(SIGCHLD, (struct sigaction *) &zact, (struct sigaction *)0);

    // checking if signal control C is pressed and then exiting
    if ((sigaction(SIGINT, &act1, NULL) != 0)) //
    {
        perror("ERROR in Sigaction");
        exit(0);
    }

    //Call to socket function using address family and socket connection
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // Check if socket returns value less then 0
    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        exit(1);
    }

    /* Initialize socket structure */
    // Define socket structure and use b zero to set all sockets to NULL
    bzero((char *)&serv_addr, sizeof(serv_addr));
    //Server address is structured using address family
    serv_addr.sin_family = AF_INET;
    // Server addreessed set accdirListingting all IP address
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    // Set port number to host to network short
    serv_addr.sin_port = htons(portno);

    //Bind socketfd with the server address using Bind function
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        // Print error on binding
        perror("ERROR on binding");
        exit(1);
    }
    socklen_t len = sizeof(serv_addr);
    if (getsockname(sockfd, (struct sockaddr *)&serv_addr, &len) == -1)
        perror("ERROR on getsockname");
    else
        fprintf(stdout, "Server is listening on port: %d\n", ntohs(serv_addr.sin_port));
    // Print to log file that server is now listening
    /*   fprintf(logfile, "Server is listening\n\n");
fflush(logfile);*/

    // Listen for socket fd and can accdirListingt a log of MAX_CLIENTS connections
    if (listen(sockfd, MAX_CLIENTS) < 0)
        perror("ERROR on listen");

    // Store client address length into clilen
    clilen = sizeof(cli_addr);

    // Infinite loop so clients can continue to connect
    while (1)
    {
        //accepts the connection of the next available client based on the client address
        if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen) < 0))
        {
            perror("ERROR on accept");
            exit(1);
        }

        // Create a child process
        pid = fork();

        // If error on fork then print to log file
        if (pid < 0)
        {
            perror("ERROR on fork");
            exit(1);
        }

        if (pid == 0)
        {
            //Creating client process
            close(sockfd);

            /*         fprintf(logfile, "Client process starting\n\n");
      fflush(logfile);*/
            // Function call to do process sending sock fd
            processrequest(newsockfd);
            exit(0);
        }
        else
        {
            close(newsockfd); //close the socket
        }

    } /* end of while */
      // Declaring integer for sockfd, int to hold new socket, int to hold port numbers and int to client length
}


void writelog(char *method, char *host, char *resource, int status)
{
    //https://stackoverflow.com/questions/9101590/fprintf-and-ctime-without-passing-n-from-ctime
    char s[1000];

    time_t t = time(NULL);
    struct tm *p = localtime(&t);

    strftime(s, 1000, "%a, %d %b %Y %H:%M:%S %Z", p);
    fprintf(logfile, "[ %s ] %s %s %s %d\n", s, method, host, resource, status);
    fflush(logfile);
}

void processrequest(int sock)
{

    // Integer holding write value
    int n;
    //array to contain the first 2 lines of the request
    // Char buffer size BUFF_SIZE holding header
    char buffer[BUFF_SIZE];
    //Setting all values in buffer to zero
    bzero(buffer, BUFF_SIZE);
    //Read in from sock to buffer with size of BUFF_SIZE
    n = read(sock, buffer, BUFF_SIZE);
    // if read in value is less then 0 then print error
    if (n < 0)
    {
        perror("ERROR reading from socket");
        exit(1);
    }
    // printf("%s\n", buffer);
    //duplicate the request, this is to be used later in the trace method
    char *requestDuplicate = malloc(strlen(buffer) + 1);
    strcpy(requestDuplicate, buffer);
    //tokenise the request
    char *requestToken = NULL;
    //used to  preserve the state of the token
    char *requestTokenSave;

    //get the first line which contains the method, resource and version
    requestToken = strtok_r(buffer, "\n", &requestTokenSave);
    char *statusToken;
    //used to  preserve the state of the token
    char *statusTokenSave;
    statusToken = strtok_r(requestToken, " ", &statusTokenSave);

    //get the second line which contains the host name
    requestToken = strtok_r(NULL, "\n", &requestTokenSave);
    char *hostToken;
    //used to  preserve the state of the token
    char *hostTokenSave;
    hostToken = strtok_r(requestToken, " ", &hostTokenSave);
    //move forward, this contains the actual host name now
    hostToken = strtok_r(NULL, " ", &hostTokenSave);
    //remove the newline character
    hostToken[strcspn(hostToken, "\r\n")] = 0;

    if (strcmp(statusToken, "GET") == 0)
    {
        //get resource token
        statusToken = strtok_r(NULL, " ", &statusTokenSave);
        request(sock, statusToken, hostToken, 0);
    }
    else if (strcmp(statusToken, "TRACE") == 0)
    {
        //get resource token
        statusToken = strtok_r(NULL, " ", &statusTokenSave);
        //send request token to trace method
        trace(sock, statusToken, hostToken, requestDuplicate);
        // trace(sock, statusToken, hostToken, 0);
    }
    else if (strcmp(statusToken, "HEAD") == 0)
    {
        //get resource token
        statusToken = strtok_r(NULL, " ", &statusTokenSave);
        request(sock, statusToken, hostToken, 1);
    }
    else
    {
        //method not supported
    }
    // Check if value of n is less then 0 if so wrote  error
    // if (n < 0)
    // {
    //     /*      fprintf(logfile, "Error writing to socket\n\n");
    //   fflush(logfile);*/
    //     exit(1);
    // }
}
void writeHeader(int sock, int status, char *statusMessage, char *contentType, int contentLength, char* extras)
{
    char buffer[BUFF_SIZE];
    char s[1000];

    time_t t = time(NULL);
    struct tm *p = localtime(&t);

    strftime(s, 1000, "%a, %d %b %Y %H:%M:%S %Z", p);
    sprintf(buffer, "HTTP/1.1 %d %s\nDate: %s\nContent-Type: %s\nContent-Length: %d%s\n\n", status, statusMessage, s, contentType, contentLength, extras);
    write(sock, buffer, strlen(buffer));
}
void request(int sock, char *resource, char *host, int headOnly)
{

    char buffer[BUFF_SIZE];
    long n;
    int file_fd;
    //check for directory requests
    struct stat s;
    char *method = (headOnly) ? "HEAD" : "GET";
    //get relative path of resource
    char *rpath = (char *)malloc(1 + strlen(".") + strlen(resource));
    strcpy(rpath, ".");
    strcat(rpath, resource);

    if (stat(rpath, &s) == 0)
    {
        if (s.st_mode & S_IFDIR)
        {

            //check for parent directory requests.
            if (strstr(rpath, "..") != NULL)
            {
                fprintf(stdout, "Cannot process parent directory requests\n");

                serve400(sock, headOnly);
                writelog(method, host, resource, 400);
            }
            else
            {
                if (chdir(rpath) == -1)
                {
                    fprintf(stderr, "ERROR: Can't change to directory %s\n", rpath);
                }
                else
                {
                    //try to open index.html -> index.htm -> default.htm else show the directory lisiting

                    if ((file_fd = open("index.html", O_RDONLY)) == -1)
                    {
                        //Cant find index.html
                        if ((file_fd = open("index.htm", O_RDONLY)) == -1)
                        {
                            //Cant find index.htm
                            if ((file_fd = open("default.htm", O_RDONLY)) == -1)
                            {
                                //Cant find default.htm
                            }
                        }
                    }

                    //could not open any of the files above. Serve the dir listing
                    //https://stackoverflow.com/questions/12489/how-do-you-get-a-directory-listing-in-c
                    if (file_fd == -1)
                    {

                        
                        sprintf(buffer, "HTTP/1.1 200 OK\nContent-Type: %s\n\n", "text/html");
                        write(sock, buffer, strlen(buffer));
                        if (!headOnly)
                        {
                            //calculate the base path
                            char *basePath = (char *)malloc(1 + strlen("./") + strlen(resource));

                            if (rpath[strlen(rpath) - 1] != '/')
                            {
                                strcpy(basePath, rpath);
                                strcat(basePath, "/");
                            }
                            else
                            {
                                strcpy(basePath, rpath);
                                //remove the end slash
                                basePath[strlen(basePath) - 1] = 0;
                            }

                            sprintf(buffer, "<!DOCTYPE html>\n"
                                            "<html>\n"
                                            " <head>\n"
                                            "  <meta charset='utf-8'>\n"
                                            "  <title>Directory Listing</title>\n"
                                            "  <base href='%s'>\n"
                                            " </head>\n"
                                            " <body>\n"
                                            "  <h1>Directory listing for %s</h1>\n"
                                            "  <ul>\n",
                                    basePath, rpath);

                            write(sock, buffer, strlen(buffer));
                            DIR *dir;
                            struct dirent *dirListing;
                            //we're already in the current directory
                            dir = opendir("./");

                            if (dir != NULL)
                            {
                                while (dirListing = readdir(dir))
                                {
                                    sprintf(buffer, "   <li><a href=\"%s\">%s</a></li>\n", dirListing->d_name, dirListing->d_name);
                                    write(sock, buffer, strlen(buffer));
                                }

                                sprintf(buffer, "  </ul>\n"
                                                " </body>\n"
                                                "</html>\n");
                                write(sock, buffer, strlen(buffer));
                                writelog(method, host, resource, 200);
                                closedir(dir);
                                free(basePath);
                            }
                            else
                                perror("Couldn't open the directory");
                        }
                        writelog(method, host, resource, 200);
                    }
                    else
                    {
                        //we know the above files will be html
                        sprintf(buffer, "HTTP/1.1 200 OK\nContent-Type: %s\n\n", "text/html");
                        write(sock, buffer, strlen(buffer));
                        if (!headOnly)
                        {
                            while ((n = read(file_fd, buffer, BUFF_SIZE)) > 0)
                            {
                                write(sock, buffer, n);
                            }
                            writelog(method, host, resource, 200);
                        }
                    }
                }
            }
        }
        else if (s.st_mode & S_IFREG)
        {
            // fprintf(stdout, "it's a file");
            //TODO: Check mime types supporting here
            if ((file_fd = open(rpath, O_RDONLY)) == -1)
            {
            }
            sprintf(buffer, "HTTP/1.1 200 OK\nContent-Type: %s\n\n", "fstr");
            write(sock, buffer, strlen(buffer));

            while ((n = read(file_fd, buffer, BUFF_SIZE)) > 0)
            {
                write(sock, buffer, n);
            }
            writelog(method, host, resource, 200);
        }
        else
        {
            fprintf(stderr, "Server could not process file");
            serve400(sock, headOnly);
            writelog(method, host, resource, 400);
        }
    }
    else
    {
        fprintf(stdout, "Error locating resource");
        serve404(sock, headOnly);
        writelog(method, host, resource, 404);
    }
    free(rpath);
}
void serve400(int sock, int headOnly)
{
    char buffer[BUFF_SIZE];
    sprintf(buffer, "HTTP/1.1 400 Bad Request\n"
                    "Content-Type: %s\n\n",
            "text/html");
    write(sock, buffer, strlen(buffer));
    if (!headOnly)
    {
        sprintf(buffer, "<!DOCTYPE HTML>"
                        "<html>"
                        " <head>"
                        "  <title>400 Bad Request</title>"
                        " </head>"
                        " <body>"
                        "  <h1>Bad Request</h1>"
                        "  <p>The server could not process the request<p>"
                        " </body>"
                        "</html>");
        write(sock, buffer, strlen(buffer));
    }
}
void serve404(int sock, int headOnly)
{
    char buffer[BUFF_SIZE];
    sprintf(buffer, "HTTP/1.1 404 Not Found\n"
                    "Content-Type: %s\n\n",
            "text/html");
    write(sock, buffer, strlen(buffer));
    if (!headOnly)
    {
        sprintf(buffer, "<!DOCTYPE HTML>"
                        "<html>"
                        " <head>"
                        "  <title>400 Bad Request</title>"
                        " </head>"
                        " <body>"
                        "  <h1>Bad Request</h1>"
                        "  <p>The server could not process the request<p>"
                        " </body>"
                        "</html>");
        write(sock, buffer, strlen(buffer));
    }
}

void trace(int sock, char *resource, char *host, char *echo)
{
    char buffer[BUFF_SIZE];
    sprintf(buffer, "HTTP/1.1 200 OK\n"
                    "Content-Type: %s\n\n",
            "message/http");
    write(sock, buffer, strlen(buffer));

    sprintf(buffer, "%s", echo);
    write(sock, buffer, strlen(buffer));
    writelog("TRACE", host, resource, 200);
}

/**
 * @brief      Catches and handles exit signals
 *
 * @param[in]  signo  The signal number
 */
void catch (int signo)
{
    /*   fprintf(logfile, "Server is Shutting down\n\n");
   fflush(logfile);*/
}

/**
 * @brief      Catches and handles pid that may become zombies
 *
 * @param[in]  No input parameters
 */
void claim_zombie()
{
    // set process ID to 1
    pid_t pid = 1;
    // check process ID is greater then 0
    while (pid > 0)
    {
        // wait for process id to stop
        pid = waitpid(0, (int *)0, WNOHANG);
    }
}

/**
 * @brief      Turn server code into a daemon 
 *
 * @param[in]  No input parameters
 */
int daemon_init(void)
{
    pid_t pid;
    if ((pid = fork()) < 0)
        return (-1);

    else if (pid != 0)
    {
        printf("Server pid = %d\n", pid);
        exit(0);
    }

    setsid();
    chdir("/");
    umask(0);

    return (0);
}