#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <exception>
#include <pthread.h>
#include <vector>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <queue>
#include <time.h>

#define DATE_BUFF 1000
#define NO_THREADS 40
#define BUF_SIZE 4096
#define MAX_QS 40
#define LISTEN_SIZE 20
using namespace std;

queue<int> socket_queue;
pthread_mutex_t lock_queue;
pthread_cond_t condition_full_queue, condition_empty_queue;
void *httpresponse(void *arg);

char *generate_date(char *buf, int length_buffer)
{
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    strftime(buf, length_buffer, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    return buf;
}

void createThreads()
{

    for (int i = 0; i < NO_THREADS; i++)
    {
        pthread_t p;
        int rc = pthread_create(&p, NULL, httpresponse, NULL);
    }
}

string readFileContent(string filepath)
{

    ifstream inFileData;
    string str = "";
    string inputstr = "";      // variable for input value
    inFileData.open(filepath); // opens the file
    if (!inFileData)
    {
        return "";
    }
    while (getline(inFileData, str))
    {
        inputstr += str + "\n";
    }
    inFileData.close();
    return inputstr;
}

vector<string> split(const string &s, char delim)
{
    vector<string> elems;

    stringstream ss(s);
    string item;

    while (getline(ss, item, delim))
    {
        if (!item.empty())
            elems.push_back(item);
    }

    return elems;
}

struct HTTP_Request
{
    string HTTP_version;

    string method;
    string url;

public:
    HTTP_Request(string request) // string request
    {
        vector<string> lines = split(request, '\n');
        vector<string> first_line = split(lines[0], ' ');

        vector<string>::iterator it;
        int index;
        for (index = 0, it = first_line.begin(); it != first_line.end(); it++, index++)
        {
            switch (index)
            {
            case 0:
                this->method = *it;
                break;
            case 1:
                this->url = *it;
                break;
            case 2:
                vector<string> versionVector = split(*it, '/');
                this->HTTP_version = versionVector.at(1);
                break;
            }
        }
        this->HTTP_version = "1.0"; // We'll be using 1.0 irrespective of the request

        if (this->method != "GET")
        {
            cerr << "Method '" << this->method << "' not supported" << endl;
            exit(1);
        }
    }
};

struct HTTP_Response
{
    string HTTP_version; // 1.0 for this assignment

    string status_code; // ex: 200, 404, etc.
    string status_text; // ex: OK, Not Found, etc.

    string content_type;
    string content_length;

    string date;
    string body;

    // TODO : Add more fields if and when needed

    string get_string() // Returns the string representation of the HTTP Response
    {
        return "HTTP/" + this->HTTP_version + " " + this->status_code + " " + this->status_text + "\r\n" + "Date: " + this->date + "\r\n" +
               "Content-Length: " + this->content_length + "\r\n" + "Content-Type: " + this->content_type + "\r\n\n" + this->body;
    }
};

HTTP_Response *handle_request(string request);

HTTP_Response *handle_request(string req)
{
    HTTP_Request *request = new HTTP_Request(req);

    HTTP_Response *response = new HTTP_Response();

    response->HTTP_version = "1.0";
    char buf[DATE_BUFF];
    response->date = generate_date(buf, DATE_BUFF);
    struct stat sb;

    // if(starts request->url)
    string url = string("html_files") + request->url;
    if (stat(url.c_str(), &sb) == 0) // requested path exists
    {
        response->status_code = "200";
        response->status_text = "OK";
        response->content_type = "text/html";

        string body;

        if (S_ISDIR(sb.st_mode))
        {
            response->body = readFileContent(url + "/index.html");
            response->content_length = to_string(response->body.size());
        }

        else
        {
            response->body = readFileContent(url);
            response->content_length = to_string(response->body.size());
        }
        /*
        TODO : open the file and read its contents
        */

        /*
        TODO : set the remaining fields of response appropriately
        */
    }

    else
    {
        string output_html_404 = "<html><body><h1>404 Not Found</h1></body></html>";
        response->status_code = "404";
        response->status_text = "Not Found";
        response->content_type = "text/html";
        response->body = output_html_404;
        response->content_length = to_string(response->body.size());
        /*
        TODO : set the remaining fields of response appropriately
        */
    }

    delete request;

    return response;
}

int error(char *str)
{
    perror(str);
    exit(1);
}

void *exchangeMessages(void *arg)
{
    int *newsockfdaddr = (int *)arg;
    int newsockfd = *newsockfdaddr;

    char inbuf[256];
    do
    {
        bzero(inbuf, 256);
        read(newsockfd, inbuf, 255);
        write(newsockfd, inbuf, strlen(inbuf));
    } while (strcmp(inbuf, "quit"));
    close(newsockfd);
    return NULL;
}

void *httpresponse(void *arg)
{
    int newsockfd;
    while (true)
    {
        pthread_mutex_lock(&lock_queue);
        while (socket_queue.size() == 0)
        {
            // printf("\nQueue size empty\n");
            pthread_cond_wait(&condition_empty_queue, &lock_queue);
        }
        newsockfd = socket_queue.front();
        socket_queue.pop();
        pthread_cond_signal(&condition_full_queue);
        pthread_mutex_unlock(&lock_queue);
        try
        {
            cout << "Thread : " << gettid() << ", sockfd: " << newsockfd << "\n";
            char inbuf[BUF_SIZE];
            explicit_bzero(inbuf, BUF_SIZE);
            // printf("Before read\n");
            read(newsockfd, inbuf, BUF_SIZE - 1);

            // printf("After read: %s\n", inbuf);
            string input = inbuf;
            if (strlen(inbuf) > 0)
            {
                // printf("After assign: %ld\n", strlen(inbuf));
                HTTP_Response *response = handle_request(input);
                string output_response = response->get_string();
                write(newsockfd, output_response.c_str(), output_response.size());
            }
        }
        catch (exception e)
        {


        }

        close(newsockfd);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    
    lock_queue = PTHREAD_MUTEX_INITIALIZER;
    condition_empty_queue = PTHREAD_COND_INITIALIZER;
    condition_full_queue = PTHREAD_COND_INITIALIZER;

    int sockfd;
    if (argc != 2)
    {
        exit(1);
    }
    int portno = atoi(argv[1]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portno);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        error("Not able to bind to address");
    }

    int count_request = 0;
    createThreads();
    listen(sockfd, LISTEN_SIZE);
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t clilen = sizeof(client_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &clilen);
        count_request++;
        // cout << "*****************    " << count_request << "    *****************\n";
        // printf("sockfd: %d\n", newsockfd);
        if (newsockfd < 0)
        {
            perror("socket error");
            continue;
        }

        pthread_mutex_lock(&lock_queue);
        while (socket_queue.size() >= MAX_QS){
            // printf("\nQueue size full\n");
            pthread_cond_wait(&condition_full_queue, &lock_queue);
            
        }
        socket_queue.push(newsockfd);
        pthread_cond_signal(&condition_empty_queue);
        pthread_mutex_unlock(&lock_queue);
        // pthread_t p;
        // int rc = pthread_create(&p, NULL, httpresponse, (void *)(to_string(newsockfd).c_str()));
        // printf("rc: %d\n", rc);
    }
    // pthread_join(p,NULL);

    close(sockfd);

    return 0;
}
