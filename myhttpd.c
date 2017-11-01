#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <signal.h>
#include <dirent.h>
#include <stdarg.h>

#define BUFF_SIZE 512
#define MAX_CLIENTS 10
#define MAX_FILETYPES 100
#define MAX_LINESIZE 128
#define DEFAULT_PORT 8000
#define DEFAULT_ROOT_DIR "."
#define DEFAULT_LOG_FILE "httpd.log"
#define DEFAULT_PREFORKS 5

void processrequest(int sock);
void processDirectory(int sock, char *path, char *host, int headOnly);
void processFile(int sock, char *path, char *host, int headOnly);
void setMimeTypes(char *path);
void request(int sock, char *resource, char *host, int headOnly);
void trace(int sock, char *resource, char *host, char *echo);

void serveErr(int sock, int headOnly, int statusCode, char *statusType, char *message);

void writelogMessage(char *message, ...);
void writelogStatus(char *method, char *host, char *resource, int status);

void catch (int signo);
void claim_zombie();

int daemon_init(void);

FILE *logfile;
char *rootdir = DEFAULT_ROOT_DIR;
typedef struct
{
    char extension[MAX_LINESIZE];
    char contentType[MAX_LINESIZE];
} MIME;

int numMimes = 6;
MIME mimes[MAX_FILETYPES] = {
    {"html", "text/html"},
    {"htm", "text/html"},
    {"txt", "text/plain"},
    {"jpeg", "image/jpeg"},
    {"jpg", "image/jpeg"},
    {"gif", "image/gif"}};
/**
 * @brief      Set up socket for server to bind to and then start listening, when client has connected process will be called
 *
 * @param[in]  argc - The counter in which the arguments from command line are stories in
 *             argv - The arguments that are written into command line
 */
int main(int argc, char *argv[])
{
    int portno = DEFAULT_PORT;
    int preforks = DEFAULT_PREFORKS;
    char *logfilename = DEFAULT_LOG_FILE;
    char *mimtypeFilePath = NULL;

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
                fprintf(stderr, "ERROR: Can't Change to directory %s\r\n", optarg);
                exit(0);
            }
            break;
        case 'l':
            logfilename = optarg;
            fprintf(stdout, "Using %s as the log file\r\n", optarg);
            break;
        case 'm':
            mimtypeFilePath = optarg;
            fprintf(stdout, "Using %s as the supported mime type file\r\n", optarg);
            break;
        case 'f':
            preforks = atoi(optarg);
            break;
        default:
            fprintf(stderr, "Usage: \r\n%s \t[ -p <port number> ]\r\n\
            \t[ -d <document root> ]\r\n\
            \t[ -l <log file> ]\r\n\
            \t[ -m <file for mime types> ]\r\n\
            \t[ -f <number of preforks> ]",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    //Open logfile file for writing overwriting the exsiting file

    if ((logfile = fopen(logfilename, "w+")) == NULL)
    {
        fprintf(stderr, "Error trying to open log file.");
        exit(1);
    }

    setMimeTypes(mimtypeFilePath);

    //check valid port no
    if (portno < 0 || portno > 65535)
    {
        fprintf(stderr, "Invalid port entered. Defaulting to port: %d\r\n", DEFAULT_PORT);
        portno = DEFAULT_PORT;
    }

    daemon_init();

    int sockfd, newsockfd, clilen;

    // Struct that holds a socket address, server address and client address
    struct sockaddr_in serv_addr, cli_addr;
    // Int holding variable and a process ID
    int n, pid;

    // Declaring the signal handler for handling zombie and control c
    struct sigaction act;
    act.sa_handler = claim_zombie;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, (struct sigaction *)&act, (struct sigaction *)0);

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
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
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
    {
        perror("ERROR on getsockname");
    }
    else
    {
        // Print to log file that server is now listening
        fprintf(stdout, "Server is listening on port: %d\r\n", ntohs(serv_addr.sin_port));
        writelogMessage("Server is listening on port: %d", ntohs(serv_addr.sin_port));
    }

    // Listen for socket fd and can accept a log of MAX_CLIENTS connections
    if (listen(sockfd, MAX_CLIENTS) < 0)
        perror("ERROR on listen");

    // Ignore SIGPIPE signal, interupted requests wont fail
    signal(SIGPIPE, SIG_IGN);
    //preforks
    for (int i = 0; i < preforks; i++)
    {
        int pid = fork();
        if (pid == 0)
        { //  child
            while (1)
            {
                //accepts the connection of the next available client based on the client address
                newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &len);
                writelogMessage("Client IP: %s connected using forked child PID: %d", inet_ntoa(cli_addr.sin_addr), getpid());
                processrequest(newsockfd);
                close(newsockfd);
                writelogMessage("Disconnected client IP: %s connection from forked child PID: %d", inet_ntoa(cli_addr.sin_addr), getpid());
            }
        }
        else if (pid > 0)
        {
            writelogMessage("Forked child pid is %d", pid);
        }
        else
        {
            perror("fork");
        }
    }

    while (1)
    {
        //accepts the connection of the next available client based on the client address
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &len);
        writelogMessage("Client IP: %s connected using main server PID: %d", inet_ntoa(cli_addr.sin_addr), getpid());
        processrequest(newsockfd);
        close(newsockfd);
        writelogMessage("Disconnected client IP: %s connection from main server PID: %d", inet_ntoa(cli_addr.sin_addr), getpid());
    }
}
//sets the supported mime types from a specified mime type file
void setMimeTypes(char *path)
{
    char buffer[BUFF_SIZE];
    //check the mime type file here
    if (path != NULL)
    {

        FILE *mimefile = fopen(path, "r");
        if (mimefile != NULL)
        {
            //reset the previous (default mimes)
            memset(mimes, 0, sizeof(mimes));

            char line[256];
            numMimes = 0;
            char *lineToken = NULL;
            //foreach line in the file set the mime array stucture accordingly
            while (fgets(line, sizeof(line), mimefile) != NULL)
            {

                strcpy(mimes[numMimes].extension, strtok(line, " "));
                strcpy(mimes[numMimes].contentType, strtok(NULL, "\r\r\n"));

                numMimes++;
            }
            fclose(mimefile);
        }
        else
        {
            perror(path);
            fprintf(stderr, "Using default mimes\r\n");
        }
    }
}
//writes an info message to the log file using a simple formatter and date
void writelogMessage(char *message, ...)
{
    va_list arg;
    char s[1000];
    char buffer[BUFF_SIZE];

    time_t t = time(NULL);
    struct tm *p = localtime(&t);

    strftime(s, 1000, "%a, %d %b %Y %H:%M:%S %Z", p);

    va_start(arg, message);
    vsprintf(buffer, message, arg);
    va_end(arg);

    fprintf(logfile, "[ %s ] [ INFO ] %s \r\n", s, buffer);

    fflush(logfile);
}
//writes the status to the log file. contains the method, host, resource, status
void writelogStatus(char *method, char *host, char *resource, int status)
{
    //https://stackoverflow.com/questions/9101590/fprintf-and-ctime-without-passing-n-from-ctime
    char s[1000];

    time_t t = time(NULL);
    struct tm *p = localtime(&t);

    strftime(s, 1000, "%a, %d %b %Y %H:%M:%S %Z", p);
    fprintf(logfile, "[ %s ] %s %s %s %d\r\n", s, method, host, resource, status);
    fflush(logfile);
}

//process the request on the nominated socket
void processrequest(int sock)
{

    int n;
    // Char buffer size BUFF_SIZE holding header
    char buffer[BUFF_SIZE];
    //Setting all values in buffer to zero
    bzero(buffer, BUFF_SIZE);
    //Read in from sock to buffer with size of BUFF_SIZE
    // if read in value is less then 0 then print error
    if ((read(sock, buffer, BUFF_SIZE)) < 0)
    {
        perror("ERROR reading from socket");
        return;
    }
    //duplicate the request, this is to be used later in the trace method
    char *requestDuplicate = malloc(strlen(buffer) + 1);
    strcpy(requestDuplicate, buffer);
    //tokenise the request
    char *requestToken = NULL;
    //used to  preserve the state of the token
    char *requestTokenSave;

    //get the first line which contains the method, resource and version
    requestToken = strtok_r(buffer, "\r\n", &requestTokenSave);
    char *statusToken;
    //used to  preserve the state of the token
    char *statusTokenSave;
    statusToken = strtok_r(requestToken, " ", &statusTokenSave);

    //get the second line which contains the host name
    //Assume the host will always be the second line of the request. this might not always be the case.
    //Needs to be considered
    requestToken = strtok_r(NULL, "\r\n", &requestTokenSave);
    char *hostToken;
    //used to  preserve the state of the token
    char *hostTokenSave;

    hostToken = strtok_r(requestToken, " ", &hostTokenSave);
    //make sure the host is actually available, otherwise return nothing
    //issue when the host is not infact the second line
    if (strcasecmp(hostToken, "HOST:") != 0)
    {
        serveErr(sock, 0, 400, "Bad Request", "The server could not process the request");
        writelogStatus(statusToken, "NO HOST PROVIDED", "", 400);
    }
    else
    {
        //move forward, this contains the actual host name now
        hostToken = strtok_r(NULL, " ", &hostTokenSave);
        //remove the newline character
        hostToken[strcspn(hostToken, "\r\n")] = 0;

        if (strcasecmp(statusToken, "GET") == 0)
        {
            //get resource token
            statusToken = strtok_r(NULL, " ", &statusTokenSave);
            request(sock, statusToken, hostToken, 0);
        }
        else if (strcasecmp(statusToken, "TRACE") == 0)
        {
            //get resource token
            statusToken = strtok_r(NULL, " ", &statusTokenSave);
            //send request token to trace method
            trace(sock, statusToken, hostToken, requestDuplicate);
            // trace(sock, statusToken, hostToken, 0);
        }
        else if (strcasecmp(statusToken, "HEAD") == 0)
        {
            //get resource token
            statusToken = strtok_r(NULL, " ", &statusTokenSave);
            request(sock, statusToken, hostToken, 1);
        }
        else
        {
            //method not supported
            serveErr(sock, 0, 405, "Method Not Allowed", "The server could not process the requested method");

            writelogStatus(statusToken, hostToken, strtok_r(NULL, " ", &statusTokenSave), 405);
        }
    }

    free(requestDuplicate);
}
void writeHeader(int sock, int status, char *statusMessage, char *contentType)
{
    char buffer[BUFF_SIZE];
    char s[1000];

    time_t t = time(NULL);
    struct tm *p = localtime(&t);

    strftime(s, 1000, "%a, %d %b %Y %H:%M:%S %Z", p);
    sprintf(buffer, "HTTP/1.1 %d %s\r\nDate: %s\r\nContent-Type: %s\r\n\r\n", status, statusMessage, s, contentType);
    write(sock, buffer, strlen(buffer));
}
//algorithm used to decode the url. Found at
//https://www.rosettacode.org/wiki/URL_decoding#C
int ishex(int x)
{
    return (x >= '0' && x <= '9') ||
           (x >= 'a' && x <= 'f') ||
           (x >= 'A' && x <= 'F');
}
int decode(const char *s, char *dec)
{
    char *o;
    const char *end = s + strlen(s);
    int c;

    for (o = dec; s <= end; o++)
    {
        c = *s++;
        if (c == '+')
            c = ' ';
        else if (c == '%' && (!ishex(*s++) ||
                              !ishex(*s++) ||
                              !sscanf(s - 2, "%2x", &c)))
            return -1;

        if (dec)
            *o = c;
    }

    return o - dec;
}
//does eithe ra GET or HEAD request and returns the whole body or just teh header based on the head only varialbe
void request(int sock, char *resource, char *host, int headOnly)
{

    // char buffer[BUFF_SIZE];
    long n;
    // int file_fd;
    //check for directory requests
    struct stat s;
    char *method = (headOnly) ? "HEAD" : "GET";
    //get relative path of resource
    char *rpath = (char *)malloc(1 + strlen(".") + strlen(resource));
    //to hold the decoded resource
    char decoded[strlen(resource) + 1];

    //need to decode the url to remove any special characters such as %20
    decode(resource, decoded);
    //reate a relative path
    strcpy(rpath, ".");
    strcat(rpath, decoded);

    // fprintf(stderr, "%s", rpath);
    if (stat(rpath, &s) == 0)
    {
        //directory
        if (s.st_mode & S_IFDIR)
        {
            //process directory
            processDirectory(sock, resource, host, headOnly);
        }
        else if (s.st_mode & S_IFREG)
        {
            //file
            processFile(sock, resource, host, headOnly);
        }
        else
        {
            //cant process
            serveErr(sock, headOnly, 400, "Bad Request", "The server could not process the request");
            writelogStatus(method, host, resource, 400);
        }
    }
    else
    {
        //anything else
        //fprintf(stdout, "Error locating resource");
        serveErr(sock, headOnly, 404, "Not Found", "The server could not locate the requested resource");

        writelogStatus(method, host, resource, 404);
    }
    free(rpath);
}

//serves an error message based on the type of status
void serveErr(int sock, int headOnly, int statusCode, char *statusType, char *message)
{
    char buffer[BUFF_SIZE];

    writeHeader(sock, statusCode, statusType, "text/html");
    if (!headOnly)
    {
        sprintf(buffer, "<!DOCTYPE HTML>\r\n"
                        "<html>\r\n"
                        " <head>\r\n"
                        "  <title>%d %s</title>\r\n"
                        " </head>\r\n"
                        " <body>\r\n"
                        "  <h1>Bad Request</h1>\r\n"
                        "  <p>%s<p>\r\n"
                        " </body>\r\n"
                        "</html>\r\n",
                statusCode, statusType, message);
        write(sock, buffer, strlen(buffer));
    }
}
//does the file processing
void processFile(int sock, char *resource, char *host, int headOnly)
{
    long n;
    char buffer[BUFF_SIZE];
    char *method = (headOnly) ? "HEAD" : "GET";
    int file_fd;
    char *rpath = (char *)malloc(1 + strlen(".") + strlen(resource));
    char decoded[strlen(resource) + 1];
    //decode the url
    decode(resource, decoded);
    //create relative path
    strcpy(rpath, ".");
    strcat(rpath, decoded);
    // fprintf(stdout, "it's a file");

    //get the extension using the strchr call.
    char *ext = strrchr(resource, '.');
    char *contentType = NULL;
    if (!ext)
    {
        //no extension
        serveErr(sock, headOnly, 400, "Bad Request", "The server could not process the request");
        writelogStatus(method, host, resource, 400);
        return;
    }
    else
    {
        for (int i = 0; i < numMimes; i++)
        {
            //check if the extension exsits in the supported mime type
            if (strcasecmp(ext + 1, mimes[i].extension) == 0)
            {
                contentType = mimes[i].contentType;
                break;
            }
        }
    }
    //in mime type found
    if (contentType != NULL)
    {
        if ((file_fd = open(rpath, O_RDONLY)) == -1)
        {
            serveErr(sock, headOnly, 500, "Internal Server Error", "The server encountered an internal error");

            writelogStatus(method, host, resource, 500);
        }
        writeHeader(sock, 200, "OK", contentType);
        //write the rest of the data if not a HEAD request
        if (!headOnly)
        {
            while ((n = read(file_fd, buffer, BUFF_SIZE)) > 0)
            {
                write(sock, buffer, n);
            }
        }

        writelogStatus(method, host, resource, 200);
    }
    else
    {
        //mime type not supported
        serveErr(sock, headOnly, 415, "Unsupported Media Type", "The requested resource is unsupported");

        writelogStatus(method, host, resource, 415);
    }

    free(rpath);
}
//converts a byte to a readable format. extracted from
//http://programanddesign.com/cpp/human-readable-file-size-in-c/

char* readable_fs(double size/*in bytes*/, char *buf) {
    int i = 0;
    const char* units[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    while (size > 1024) {
        size /= 1024;
        i++;
    }
    sprintf(buf, "%.*f %s", i, size, units[i]);
    return buf;
}
//processes the directory request
void processDirectory(int sock, char *resource, char *host, int headOnly)
{
    long n;
    char buffer[BUFF_SIZE];
    int file_fd;
    struct stat statbuf;
    char m_time[32], size[16];

    //check for directory requests
    char *method = (headOnly) ? "HEAD" : "GET";
    //get relative path of resource
    char *rpath = (char *)malloc(1 + strlen(".") + strlen(resource));
    //decode the resource and construct a relative path
    char decoded[strlen(resource) + 1];
    decode(resource, decoded);
    strcpy(rpath, ".");
    strcat(rpath, decoded);
    //calculate the base path
    char *basePath = (char *)malloc(1 + strlen("//") + strlen(resource));
    strcpy(basePath, resource);
    char *tmp = (char *)malloc(1 + BUFF_SIZE + strlen(basePath));
    //if there is no trailing slash, add one to the base path
    if (resource[strlen(resource) - 1] != '/')
    {
        strcat(basePath, "/");
    }
    //check for parent directory requests.
    if (strstr(rpath, "..") != NULL)
    {
        //cannot process parent directory from root requets
        serveErr(sock, headOnly, 400, "Bad Request", "The server could not process the request");

        writelogStatus(method, host, resource, 400);
    }
    else
    {
        file_fd = -1;

        //try to open index.html -> index.htm -> default.htm else show the directory lisiting

        //create path for index.html
        strcpy(tmp, ".");
        strcat(tmp, basePath);
        strcat(tmp, "index.html");

        if ((file_fd = open(tmp, O_RDONLY)) == -1)
        {
            //couldnt find index.html
            //create path for index.htm
            memset(tmp, 0, strlen(tmp));
            strcpy(tmp, ".");
            strcat(tmp, basePath);
            strcat(tmp, "index.htm");

            if ((file_fd = open(tmp, O_RDONLY)) == -1)
            {
                //couldnt find index.htm
                //create path for default.htm
                memset(tmp, 0, strlen(tmp));
                strcpy(tmp, ".");
                strcat(tmp, basePath);
                strcat(tmp, "default.htm");
                //Cant find index.htm
                if ((file_fd = open(tmp, O_RDONLY)) == -1)
                {
                    //Cant find default.htm
                }
            }
        }

        //could not open any of the files above. Serve the dir listing
        //does not differenciate between directory and files
        //https://stackoverflow.com/questions/12489/how-do-you-get-a-directory-listing-in-c
        if (file_fd == -1)
        {
            writeHeader(sock, 200, "OK", "text/html");

            if (!headOnly)
            {

                sprintf(buffer, "<!DOCTYPE html>\r\n"
                                "<html>\r\n"
                                " <head>\r\n"
                                "  <meta charset='utf-8'>\r\n"
                                "  <title>Directory Listing</title>\r\n"
                                "  <base href='%s'>\r\n"
                                "  <style>\r\n"
                                "   td{padding: 0 20px 0 0;}\r\n"
                                "  </style>\r\n"                                
                                " </head>\r\n"
                                " <body>\r\n"
                                "  <h1>Directory listing for %s</h1>\r\n"
                                "  <table>\r\n",
                        basePath, rpath);

                write(sock, buffer, strlen(buffer));
                DIR *dir;
                struct dirent *dirListing;
                //we're already in the current directory
                dir = opendir(rpath);

                if (dir != NULL)
                {
                    int hasFiles = 0;
                    while (dirListing = readdir(dir))
                    {
                        //skip the . and .. listings
                        if(!strcmp(dirListing->d_name, ".") || !strcmp(dirListing->d_name, "..")){
                            continue;
                        }
                        //reset the tmp string
                        //create a string containing the relative path for each directory file
                        memset(tmp, 0, strlen(tmp));
                        strcpy(tmp, ".");
                        strcat(tmp, basePath);
                        strcat(tmp, dirListing->d_name);
                        
                        //open the file and stat it to get details 
                        if ((file_fd = open(tmp, O_RDONLY)) == -1){
                            perror(dirListing->d_name);
                            continue;
                        }
                        fstat(file_fd, &statbuf);
                        
                        //display the time                        
                        strftime(m_time, sizeof(m_time),
                                 "%Y-%m-%d %H:%M", localtime(&statbuf.st_mtime));

                        //if the file is a file or a directory
                        if(S_ISREG(statbuf.st_mode) || S_ISDIR(statbuf.st_mode)){
                            //append a / if its a directory
                            char *d = S_ISDIR(statbuf.st_mode) ? "/" : "";
                            //get the size of the file uisng the mentioned readable_fs function 
                            char size[15];
                            sprintf(size, "%s", readable_fs(statbuf.st_size, buffer));
                            //no need to get the size if its a directory
                            if(S_ISDIR(statbuf.st_mode))
                            {
                                sprintf(size,"[DIR]");
                            } 
                            //has files to check if there are no files
                            hasFiles++;
                            //serve it as a table
                            sprintf(buffer, "   <tr><td><a href=\"%s\">%s%s</a></td><td>%s</td><td>%s</td></tr>\r\n",
                            dirListing->d_name, dirListing->d_name, d, m_time, size);
                            write(sock, buffer, strlen(buffer));
                        }
                        // char *d = S_ISDIR(statbuf.st_mode) ? "/" : "";

                        // sprintf(buffer, "   <li><a href=\"%s\">%s%s</a></li>\r\n", dirListing->d_name, dirListing->d_name, d);
                        // write(sock, buffer, strlen(buffer));
                    }
                    //no files, just serve a blank table
                    if(!hasFiles)
                    {
                        sprintf(buffer, "   <tr><td>No files found</td></tr>\r\n");
                        write(sock, buffer, strlen(buffer));
                    }
                    sprintf(buffer, "  </table>\r\n"
                                    " </body>\r\n"
                                    "</html>\r\n");
                    write(sock, buffer, strlen(buffer));
                    writelogStatus(method, host, resource, 200);
                    closedir(dir);
                }
                else
                {
                    perror("Couldn't open the directory");
                }
            }
            writelogStatus(method, host, resource, 200);
        }
        else
        {
            //we know the above files will be html
            writeHeader(sock, 200, "OK", "text/html");

            if (!headOnly)
            {
                while ((n = read(file_fd, buffer, BUFF_SIZE)) > 0)
                {
                    write(sock, buffer, n);
                }
                writelogStatus(method, host, resource, 200);
            }
        }
        // }
    }
    free(tmp);
    free(basePath);
    free(rpath);
}
//echos back a user request
void trace(int sock, char *resource, char *host, char *echo)
{
    char buffer[BUFF_SIZE];
    writeHeader(sock, 200, "OK", "message/http");

    //send the request back in the response
    sprintf(buffer, "%s", echo);
    write(sock, buffer, strlen(buffer));
    writelogStatus("TRACE", host, resource, 200);
}

//Catches and handles pid that may become zombies

void claim_zombie()
{
    pid_t pid = 1;
    // set process ID to 1 pid_t pid = 1;
    // check process ID is greater then 0
    while (pid > 0)
    {
        // wait for process id to stop
        pid = waitpid(0, (int *)0, WNOHANG);
    }
}

//Turn server code into a daemon

int daemon_init(void)
{
    pid_t pid;
    if ((pid = fork()) < 0)
        return (-1);

    else if (pid != 0)
    {
        printf("Server pid = %d\r\n", pid);
        writelogMessage("Server pid = %d", pid);
        exit(0);
    }

    setsid();
    chdir(DEFAULT_ROOT_DIR);
    umask(0);

    return (0);
}