#ifndef _A4_OUTPUT_STREAM_H_
#define _A4_OUTPUT_STREAM_H_

#include <string>
#include <vector>
#include <cassert>
#include <set>
#include <map>

#include <a4/types.h>


namespace google {
namespace protobuf {
    class Descriptor;
    class Message;
    
    namespace io {
        class ZeroCopyOutputStream;
        class FileOutputStream;
        class GzipOutputStream;
        class CodedOutputStream;
    }
    class SimpleDescriptorDatabase;
}
}


namespace a4 {
namespace io {

    class A4Message;
    class BaseCompressedOutputStream;

    /// Class to write Messages to files or streams.
    /// To write a message belonging to a certain class
    /// make sure you told @UseClassID about it,
    /// by writing "template UseClassId<MyProtobufClass>;"
    /// somewhere in a cpp file.
    class OutputStream
    {
        public:
            OutputStream(const std::string &output_file,
                           const std::string description="");
            OutputStream(shared<google::protobuf::io::ZeroCopyOutputStream>,
                           const std::string outname="<stream>",
                           const std::string description=""); 
            ~OutputStream();

            /// Write a message to the stream
            bool write(const google::protobuf::Message& m);
            bool write(shared<const A4Message> m);

            /// Write a metadata message to the stream
            /// Take care to respect the metadata message direction - forward means the events following
            bool metadata(const google::protobuf::Message& m);
            bool metadata(shared<const A4Message> m);

            /// \internal Explicitly open the file. \endinternal
            bool open();
            /// \internal Explicitly close the file. \endinternal
            bool close();
            bool opened() { return _opened; };
            bool closed() { return _closed; };
            
            typedef enum {
                UNCOMPRESSED,
                ZLIB,
                SNAPPY,
                LZ4,
            } CompressionType;
            static CompressionType compression_type(const std::string& s) {
                if(s == "UNCOMPRESSED") return UNCOMPRESSED;
                if(s == "ZLIB") return ZLIB;
                if(s == "SNAPPY") return SNAPPY;
                if(s == "LZ4") return LZ4;
                FATAL("Unknown Compression Type: ", s);
            }
            /// Set compression type and level
            OutputStream& set_compression(CompressionType t, int level = 5);
            OutputStream& set_compression(std::string t, int level = 5) {
                return set_compression(compression_type(t), level);
            };




            /// If called, metadata will refer to the events following the metadata, instead of events before.
            /// Has to be called before writing is begun.
            OutputStream& set_forward_metadata() { assert(!_opened); _metadata_refers_forward = true; return *this; };

            /// String representation of this stream for user output
            std::string str() { return std::string("OutputStream(\"") + _output_name + "\", \"" + _description + "\")"; };
        private:
            shared<google::protobuf::io::ZeroCopyOutputStream> _raw_out;
            shared<google::protobuf::io::FileOutputStream> _file_out;
            
            UNIQUE<BaseCompressedOutputStream> _compressed_out;
            UNIQUE<google::protobuf::io::CodedOutputStream> _coded_out;


            bool write(uint32_t class_id, const google::protobuf::Message& m);
            bool write(uint32_t class_id, shared<const A4Message> m);
            bool write_header(std::string description);
            bool write_footer();
            bool start_compression();
            bool stop_compression();

            uint64_t get_bytes_written();
            void reset_coded_stream();

            std::string _output_name, _description;
            int _fileno;
            bool _compression;
            int _compression_level;

            CompressionType _compression_type;
            bool _opened, _closed;
            bool _metadata_refers_forward;
            std::vector<uint64_t> metadata_positions;
            std::vector<uint64_t> protoclass_positions;
            
            std::set<std::string> _written_file_descriptor_set;
            std::set<uint32_t> _written_classids;
            uint32_t _next_class_id;
            uint32_t _next_metadata_class_id;
            
            void write_protoclass(uint32_t class_id, const google::protobuf::Descriptor* d);
            uint32_t find_class_id(const google::protobuf::Descriptor* d, bool metadata);
            
            std::map<const std::string, uint32_t> _class_id;
            std::vector<int> _class_id_counts;
            bool have_written_classid(const uint32_t& classid) { return _written_classids.find(classid) != _written_classids.end(); }
            void set_written_classid(const uint32_t& classid) { _written_classids.insert(classid); }
    };

}
}

#endif
