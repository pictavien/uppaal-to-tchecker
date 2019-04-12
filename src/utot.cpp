/*
 * This file is part of the TChecker Project.
 *
 * See files AUTHORS and LICENSE for copyright details.
 */

#include <fstream>
#include <iostream>
#include <sstream>
#include <cassert>
#include <cstring>
#include <getopt.h>
#include <sys/errno.h>
#include <filesystem>
#include "utot-version.hh"
#include "utot-translate.hh"
#include "utot.hh"

enum file_format_t {
    FORMAT_NONE = 0,
    FORMAT_PLAIN,
    FORMAT_XML
};

static const char *FORMAT_STRINGS[] = {
    [FORMAT_NONE] = "unkwown",
    [FORMAT_PLAIN] = "plain",
    [FORMAT_XML] = "xml",
};

enum input_language_t {
    LANG_NONE = 0,
    LANG_TA,
    LANG_XTA
};

static const char *LANG_STRINGS[] = {
    [LANG_NONE] = "unkwown",
    [LANG_TA] = "ta",
    [LANG_XTA] = "xta"
};

enum ShortOption {
    SO_DEBUG = 'd',
    SO_ERASE = 'e',
    SO_HELP = 'h',
    SO_VERBOSE = 'V',
    SO_VERSION = 'v',
    SO_PLAIN = 0x100,
    SO_XML = 0x101,
    SO_TA = 0x102,
    SO_XTA = 0x103
};

int utot::verbose_level = 0;
bool utot::debug = false;

static bool show_help = false;
static bool show_version = false;
static file_format_t enforced_format = FORMAT_NONE;
static input_language_t enforced_language = LANG_NONE;
static bool erase_output = false;
static const char *input_filename = nullptr;
static bool close_input = false;
static const char *output_filename = nullptr;
static bool close_output = false;

static const char OPTION_STRING[] = {
    SO_DEBUG,
    SO_ERASE,
    SO_HELP,
    SO_VERBOSE,
    SO_VERSION,
    '\0'
};
static const struct option OPTIONS[] = {
    {"debug",   no_argument, nullptr, SO_DEBUG},
    {"erase",   no_argument, nullptr, SO_ERASE},
    {"xta",     no_argument, nullptr, SO_XTA},
    {"xml",     no_argument, nullptr, SO_XML},
    {"plain",   no_argument, nullptr, SO_PLAIN},
    {"ta",      no_argument, nullptr, SO_TA},
    {"version", no_argument, nullptr, SO_VERSION},
    {"verbose", no_argument, nullptr, SO_VERBOSE},
    {"help",    no_argument, nullptr, SO_HELP},
    {nullptr,   no_argument, nullptr, 0}
};

static void
s_usage (const char *cmd, std::ostream &out)
{
  out << "usage: " << cmd
      << " [options] [uppaal-input-file] [tchecker-output-file]" << std::endl
      << "where options are: " << std::endl
      << "--debug, -" << (char) SO_DEBUG
      << " \t enable debug traces" << std::endl

      << "--erase, -" << (char) SO_ERASE
      << " \t erase output file if it exists" << std::endl

      << "--help, -" << (char) SO_HELP
      << " \t display this help message." << std::endl

      << "--verbose, -" << (char) SO_VERBOSE
      << " \t increase the level of verbosity" << std::endl

      << "--version, -" << (char) SO_VERSION
      << " \t display version number" << std::endl

      << "--xml"
      << " \t enforce XML as input format" << std::endl

      << "--txt"
      << " \t enforce raw text as input format" << std::endl

      << "--xta"
      << " \t enforce XTA as input language" << std::endl

      << "--ta"
      << " \t enforce TA as input langue" << std::endl

      << "-- \t\t specify the end of options (if necessary)" << std::endl
      << std::endl
      << "If no input file is specified, the standard input is used."
      << std::endl
      << "If several 'xta' or 'xml' options are used the last one is used."
      << std::endl
      << std::endl;
}

class command_line_exception : public std::exception {
};

class parser_exception : public std::exception {
};

static void
s_parse_options (int argc, char **argv)
{
  int c;

  while ((c = getopt_long (argc, argv, OPTION_STRING, OPTIONS, nullptr)) != -1)
    {
      switch (c)
        {
          case SO_DEBUG:
            utot::debug = true;
          break;

          case SO_ERASE :
            erase_output = true;
          break;

          case SO_VERBOSE:
            utot::verbose_level++;
          break;

          case SO_HELP:
            show_help = true;
          break;

          case SO_TA:
            enforced_language = LANG_TA;
          break;

          case SO_XTA:
            enforced_language = LANG_XTA;
          break;

          case SO_XML:
            enforced_format = FORMAT_XML;
          break;

          case SO_PLAIN:
            enforced_format = FORMAT_PLAIN;
          break;

          case SO_VERSION:
            show_version = true;
          break;

          case ':':
            std::cerr << "missing argument to option '" << optopt << "'."
                      << std::endl;
          break;
          case '?':
          default:
            assert(c == '?');
          throw std::exception ();
        }
    }
  argc -= optind;
  argv += optind;

  if (argc > 2)
    {
      std::cerr << "exceed number of arguments." << std::endl;
      throw std::exception ();
    }

  if (argc == 1)
    input_filename = argv[0];
  if (argc == 2)
    output_filename = argv[1];
}

static input_language_t
s_compute_input_language (const char *filename, file_format_t &format)
{
  input_language_t result = enforced_language;
  format = enforced_format;

  if (filename == NULL)
    return result;

  const char *extpos = strrchr (input_filename, '.');
  if (extpos != nullptr)
    {
      if (strcmp (extpos, ".ta") == 0 || strcmp (extpos, ".ta") == 0)
        {
          result = LANG_TA;
          format = FORMAT_PLAIN;
        }
      else if (strcmp (extpos, ".xta") == 0 || strcmp (extpos, ".ta") == 0)
        {
          result = LANG_XTA;
          format = FORMAT_PLAIN;
        }
      else if (strcmp (extpos, ".xml") == 0)
        {
          result = LANG_XTA;
          format = FORMAT_XML;
        }
      else
        UTOT_VERBOSE (0, "unknown extension '%s'.\n", extpos);
    }

  if (enforced_language != LANG_NONE)
    {
      if (enforced_language != result)
        UTOT_VERBOSE (0, "warning: enforced language '%s' for a file with "
                         "extension %s.\n", LANG_STRINGS[enforced_language],
                      extpos);
      result = enforced_language;
    }

  if (enforced_format != FORMAT_NONE)
    {
      if (enforced_format != format)
        UTOT_VERBOSE (0, "warning: enforced file format '%s' for a file with "
                         "extension %s.\n", FORMAT_STRINGS[enforced_format],
                      extpos);
      result = enforced_language;
    }
  return result;
}

static bool
s_file_exists (const char *filename)
{
  FILE *input = fopen (filename, "r");
  bool result = (input != nullptr);

  if (result)
    fclose (input);
  return result;
}

static void
s_open_output_file (const char *filename, std::ostream &out)
{
  if (filename == nullptr)
    {
      output_filename = "stdout";
      out.rdbuf (std::cout.rdbuf ());
      return;
    }

  if (s_file_exists (filename) && !erase_output)
    {
      std::cerr << "output file '" << output_filename << "' already exists."
                << std::endl;
      throw std::exception ();
    }

  std::ofstream ofs (filename, std::ios_base::out);
  if (ofs.good ())
    {
      out.rdbuf (ofs.rdbuf ());
    }
  else
    {
      std::cerr << "can't open file '" << output_filename << "'." << std::endl
                << "reason: " << strerror (errno) << std::endl;
      throw std::exception ();
    }
}

static std::string
s_read_inputfile (const char *filename)
{
  std::streambuf *buf;
  std::ifstream ifs;
  std::ostringstream oss;

  if (filename == nullptr)
    buf = std::cin.rdbuf ();
  else
    {
      buf = ifs.rdbuf ()->open (filename, std::ios_base::in);
      if (buf == nullptr)
        {
          std::cerr << "can't open file '" << filename << "'." << std::endl
                    << "reason: " << strerror (errno) << std::endl;
          throw std::exception ();
        }
    }
  UTOT_VERBOSE (0, "reading input from: %s.\n",
                filename ? filename : "standard input");
  oss << buf;
  oss.flush ();
  return oss.str ();
}

int
main (int argc, char **argv)
{
  int result = EXIT_SUCCESS;

  try
    {
      s_parse_options (argc, argv);

      if (show_help)
        s_usage (argv[0], std::cout);
      else if (show_version)
        std::cout << UTOT_VERSION << std::endl;
      else
        {
          file_format_t fmt;
          input_language_t lang =
              s_compute_input_language (input_filename, fmt);

          if (lang == LANG_NONE)
            {
              std::cerr << "can not determine input language ta or xta."
                        << std::endl;
              throw std::exception ();
            }

          if (fmt == FORMAT_NONE)
            {
              UTOT_VERBOSE (1, "enforcing plain format.\n");
              fmt = FORMAT_PLAIN;
            }
          std::string input = s_read_inputfile (input_filename);

          UTAP::TimedAutomataSystem tas;

          UTOT_VERBOSE (0, "parsing %s/%s input.\n",
                        LANG_STRINGS[lang], FORMAT_STRINGS[fmt]);

          if (fmt == FORMAT_PLAIN)
            {
              if (!parseXTA (input.c_str (), &tas, lang == LANG_XTA))
                {
                  for (UTAP::error_t e : tas.getErrors ())
                    {
                      std::cerr << e << std::endl;
                    }
                  throw std::exception ();
                }
            }
          else if (parseXMLBuffer (input.c_str (), &tas, lang == LANG_XTA) < 0)
            throw std::exception ();

          if (tas.hasWarnings () && utot::verbose_level > 0)
            {
              for (UTAP::error_t e : tas.getErrors ())
                {
                  std::cout << e << std::endl;
                }
            }
          UTOT_VERBOSE (0, "translating model into tchecker file format.\n");
          std::ofstream out;
          s_open_output_file (output_filename, out);
          utot::translate_model (tas, out);
        }
    }
  catch (const std::exception &e)
    {
      if (dynamic_cast<const command_line_exception *>(&e) != nullptr)
        s_usage (argv[0], std::cerr);

      result = EXIT_FAILURE;
    }

  return result;
}