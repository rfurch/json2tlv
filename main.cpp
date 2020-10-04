//
//   Simple homework for NXLOG  (JSON to TLV)
//

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include "boost/date_time/local_time/local_time.hpp"

#include <boost/iostreams/device/mapped_file.hpp> // for mmap
#include <algorithm>  // for std::find
#include <iostream>   // for std::cout
#include <cstring>
#include <chrono>
#include <string>
#include <map>

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include <fcntl.h>

// single header json parserr. 
// https://github.com/nlohmann/json/
#include "json.hpp"

#include "main.hpp"

using namespace boost::program_options;
using namespace boost::posix_time;
using namespace boost::iostreams;
using nlohmann::json;
using namespace std;

using json = nlohmann::json;

/// basic types for TLV (proprietary definition)
enum tlvType {BYTE=1, BOOL, INTEGER, STRING};

std::uint16_t  _verbosity = 0U;

namespace io = boost::iostreams;

// -------------------------------------------------------------
// -------------------------------------------------------------

// simple class to wrap JSON file handling

class jsonFile {

  public:
    jsonFile()  {  }
    bool processFile(std::string& BOOST_PP_FILENAME_1, std::string& BOOST_PP_FILENAME_2, std::string& BOOST_PP_FILENAME_3);

  private:
    std::string fileName; 

    bool parseBWLine(const std::string& charLine, std::map<std::string, int> &strIntMap, FILE *outF);
};

// -------------------------------------------------------------

// simple function,  build TLV vector from memory block 
std::vector<uint8_t> dataToTlv( unsigned char *data, uint8_t dataType, uint16_t dataSize )   {
  std::vector<uint8_t>    tlvData;

  // build binary array / vector
  tlvData.push_back((uint8_t) dataType );
  tlvData.push_back((uint8_t) ((dataSize >> 8) & 0xff) );
  tlvData.push_back((uint8_t) (dataSize & 0xff) );
  for  (int i = 0; i < dataSize ; i++) {
      tlvData.push_back((uint8_t) ( *data++  ) );
      }
  return(tlvData);  
  } 

// -------------------------------------------------------------

// receive INDEX and VALUE and convert to TLV.  Write to binary file
// element by element.
// possible optimization would be to write to file in bigger chunks

bool toTVL(int& index, nlohmann::basic_json<>& jsonElement, FILE *outF)   {
  std::vector<uint8_t>    tlvData;
  std::vector<uint8_t>    valueTlvData;

  //  build index TLV as INTEGER
  tlvData = dataToTlv( (unsigned char *) &index, (uint8_t) INTEGER, (uint16_t) sizeof(uint32_t) ) ;

  if ( jsonElement.is_number() ) {             /// NUMBER CASE    
    if (_verbosity > 3) std::cout << " is a number " << endl;
    double d = jsonElement;  
    valueTlvData = dataToTlv( (unsigned char *) &d, (uint8_t) INTEGER, (uint16_t) sizeof(double) ) ;  
    }
  else if ( jsonElement.is_string() ) {        /// STRING CASE
    if (_verbosity > 3) std::cout << " is a STRING " << endl;
    std::string s  = jsonElement;  

    valueTlvData = dataToTlv( ((unsigned char *) s.c_str()) , (uint8_t) STRING, s.length() ) ;  
    }
  else if ( jsonElement.is_boolean() ) {      /// BOOBLEAN CASE
    unsigned char data = 1;
    if (_verbosity > 3) std::cout << " is a bool " ;
    if (jsonElement == true ) {
      data = 1;
      if (_verbosity > 3) std::cout << "  (TRUE) " << endl;
      }
    else if (jsonElement == false )  {
      data = 0;
      if (_verbosity > 3) std::cout << "  (FALSE) " << endl;
      }
    valueTlvData = dataToTlv( (unsigned char *) &data, (uint8_t) BOOL, (uint16_t) sizeof(char) );
    }
  else if ( jsonElement.is_number_float() ) 
    if (_verbosity > 3) std::cout << " is a float " << endl;
  else if ( jsonElement.is_null() ) 
    if (_verbosity > 3) std::cout << " is a NULL " << endl;

  // concatenate
  tlvData.insert(tlvData.end(), valueTlvData.begin(), valueTlvData.end());
  
  // dump TLV to file 
  fwrite( (char *) &tlvData[0], sizeof(char),  (int) tlvData.size(), outF);

  if (_verbosity > 3) {
    for  (int i = 0; i < tlvData.size(); i++) {
      std::cout << std::setw(2) << std::setfill('0') << std::hex << (0xff & tlvData[i]) << " ";
      }
    std::cout << std::endl;  
    }

}

// -------------------------------------------------------------
// parse line with following format:
// { "key4":"value2", "key2":42, "key5":true}

bool jsonFile::parseBWLine(const std::string& charLine, std::map<std::string, int> &strIntMap, FILE *outF)   {
std::map<std::string, int>::iterator  iter;
static int                            counter=0;   // consecutive ID to replace KEY
int                                   index = 0;

if ( charLine.length()<3 )
  return false;

if (_verbosity > 3)  printf(" --- %s --- \n", charLine.c_str());

// parse received line
json jsonObject = json::parse(charLine, nullptr, false);
if (jsonObject.is_discarded())   {
    std::cerr << "parse error" << std::endl;
  }
else {
  // get every element (eg: "key3": true) as pairs key ("key3")  / value  (true)
  for (auto& el : jsonObject.items())  {

    // search in MAP,  get INDEX if exists.  If not,  insert a new INDEX in MAP
    if ( (iter = strIntMap.find(el.key())) != strIntMap.end() )  {
      index = iter->second;  
      if (_verbosity > 4)     std::cout << "key: " << el.key() << ", value:" << el.value() << "  ---  found ID: "  <<  iter->second << " \n";
      }
    else  {   // Index not found
      counter++;
      index = counter;
      if (_verbosity > 4) std::cout << "key: " << el.key() << ", value:" << el.value() << "  ---  NOT found. Assigned ID: " << counter << " \n";
      strIntMap.insert( { el.key(), counter });
    }

    if (_verbosity > 3) std::cout << "key: " << el.key() << ", value:" << el.value() << '\n';

    // convert index and value to TLV 
    toTVL(index, el.value(), outF );
    }
  }

fflush(stdout);

return true;
}

// -------------------------------------------------------------

// dump string/integer dictionary (MAP) to file,  in TLV format
bool dumpMap( std::map<std::string, int> & strIntMap, FILE *fOutMap)  {
map<std::string, int>::iterator it;
std::vector<uint8_t>    tlvKeyData;
std::vector<uint8_t>    tlvValueData;

if (_verbosity > 3) std::cout << "string / integre map: " << std::endl;

for ( it = strIntMap.begin(); it != strIntMap.end(); it++ ) {

  //  build STRING key in TLV format 
  tlvKeyData = dataToTlv( ((unsigned char *) it->first.c_str()) , (uint8_t) STRING, it->first.length() ) ;  

  //  build INT value in TLV format 
  tlvValueData = dataToTlv( (unsigned char *) &(it->second), (uint8_t) INTEGER, (uint16_t) sizeof(uint32_t) ) ;

  // concatenate
  tlvKeyData.insert(tlvKeyData.end(), tlvValueData.begin(), tlvValueData.end());

  // dump TLV to file 
  fwrite( (char *) &tlvKeyData[0], sizeof(char),  (int) tlvKeyData.size(), fOutMap);

  if (_verbosity > 3)
    std::cout << it->first << ':' << it->second << std::endl ;    
  }

if (_verbosity > 3) std::cout << std::endl;

return(true);
}

// -------------------------------------------------------------

// open an read JSON file line by line.  Create / truncate output file too

bool jsonFile::processFile(std::string& inputFilename, std::string& outputFilename, std::string& mapOutputFilename) {

  std::vector<uintmax_t> vec; 
  std::map<std::string, int> strIntMap;
  FILE *fOut=NULL;
  FILE *fOutMap=NULL;

  this->fileName = inputFilename;

  // input file MUST exists
  if ( DoesFileExist(inputFilename) != true )  {
      std::cout << "\n ERROR: Unable to read the file:  " << inputFilename << '\n';
      return false;
  }

  // for TLV output
  if ( (fOut = fopen(outputFilename.c_str(), "w+b")) == NULL ){
      std::cout << "\n ERROR: Unable to create output file:  " << outputFilename << '\n';
      return false;
      }

  // for MAP tlv output
  if ( (fOutMap = fopen(mapOutputFilename.c_str(), "w+b")) == NULL ){
      std::cout << "\n ERROR: Unable to create output file:  " << mapOutputFilename << '\n';
      return false;
      }

  int fdr = open(inputFilename.c_str() , O_RDONLY);
      if (fdr >= 0) {
          io::file_descriptor_source fdDevice(fdr, io::file_descriptor_flags::close_handle);
          io::stream <io::file_descriptor_source> in(fdDevice);
          if (fdDevice.is_open()) {
              std::string line;
              while (std::getline(in, line)) {  // read lines and process them sending to TVL file

                  this->parseBWLine(line, strIntMap, fOut);
              }
              fdDevice.close();


          // dump MAP to file in TLV format    
          dumpMap( strIntMap, fOutMap);

          }
      }

  fclose(fOutMap);
  fclose(fOut);

  return true;
}

// -------------------------------------------------------------

// basic argument parser for the program

int process_program_options(const int argc, const char *const argv[], std::string& inFilename, std::string& outFilename, std::string& mapFilename)
{ 
  namespace po = boost::program_options; 

  printVersions();
  
  try
    {
      options_description desc{"Options"};
      desc.add_options()
        ("help,h", "Help screen")
        ("verbose,v", po::value<std::uint16_t> (&_verbosity)->default_value(0), "Print more verbose messages at each additional verbosity level.")
        ("outfile,o", (po::value< std::string > (&outFilename))->default_value("binaryTLV.out"), "Output (TLV) filename")
        ("mapfile,m", (po::value< std::string > (&mapFilename))->default_value("binaryTLVMap.out"), "Output (TLV) MAP  filename")
        ("infile,i", po::value< std::string > (&inFilename), "Input (JSON) filename");

      variables_map vm;
      store(parse_command_line(argc, argv, desc), vm);
      notify(vm);

      if (vm.count("help"))  {
        std::cout << desc << '\n';
        exit(1);
        }
      
      if (! vm.count("infile"))  {
        std::cout << desc << '\n';
        std::cout << "\n filename IS REQUIRED! " <<  '\n';
        exit(1); 
      }
    }
  catch (const error &ex) 
    {
      std::cerr << ex.what() << '\n';
      exit(1);
    }
}

// -------------------------------------------------------------

int main(int argc, const char *argv[])
{
    std::string               inputFilename{}; 
    std::string               outputFilename{};    // binary TVL output filename
    std::string               mapOutputFilename{};    // map TLV output filenam
    jsonFile                  jf;                  // instanciation of my own class  
    float                     threshold=0.0;

    process_program_options(argc, argv, inputFilename, outputFilename, mapOutputFilename);

    if ( ! jf.processFile(inputFilename,  outputFilename, mapOutputFilename) )
      return 1;

  return(0);
}

// -------------------------------------------------------------
// -------------------------------------------------------------
