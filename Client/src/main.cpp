#include <iostream>
#include <uv.h>
#include <string>

#include "chatsession.h"
#include "optionargs.h"

enum optionIndex { UNKNOWN, HELP, PORT, ADDRESS, NAME };
const option::Descriptor usage[] =
{
    {UNKNOWN, 0, "", "", Arg::Unknown, "USAGE: Client -p PORT -n NICKNAME -a SERVER_ADDRESS" },
    {HELP, 0, "", "help", Arg::None, "--help \t Print usage and exit." },
    {PORT, 0, "p", "port", Arg::Numeric, "-p <port>, \t --port=<port> \t(number)"},
    {ADDRESS, 0, "a", "address", Arg::Required, "-a <ip address>, \t--address=<ip address>" },
    {NAME, 0, "n", "name", Arg::NonEmpty, "-n <name>\t--name==<name>, \t cannot be empty"},
    { 0, 0, 0, 0, 0, 0 },
};

int main(int argc, char* argv[])
{
    if (argc > 0)
    {
        argc--;
        argv++;
    }
    
    option::Stats stats(usage, argc, argv);

    option::Option options[stats.options_max];
    option::Option buffer[stats.buffer_max];
    
    option::Parser parse(usage, argc, argv, options, buffer);
    if (parse.error())
    {
        fprintf(stderr, "Error while parsing options\n");
        return 1;
    }

    if (options[HELP] || argc == 0)
    {
        option::printUsage(fwrite, stderr, usage, 80);
        return 1;
    }

    bool hasOptions = true;
    if (!options[PORT])
    {
        hasOptions = false;
        fprintf(stderr, "port missing\n");
    }

    if (!options[ADDRESS])
    {
        hasOptions = false;
        fprintf(stderr, "address missing\n");
    }

    if (!options[NAME])
    {
        hasOptions = false;
        fprintf(stderr, "Name missing\n");
    }

    if (hasOptions == false)
    {
        fprintf(stderr, "Some parameters are missing!\n");
        return 1;
    }

    try
    {
        ChatSession* session = ChatSession::GetInstance();
        session->Init(std::stoi(options[PORT].arg),
                      options[ADDRESS].arg,
                      options[NAME].arg);
    }
    catch (std::exception& e)
    {
       std::cout << e.what() << std::endl;
    }

    std::cout << "Closing Application" << std::endl;
    return 0;
}
