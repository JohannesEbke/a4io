#ifndef _A4_STREAM_H_
#define _A4_STREAM_H_

#include <google/protobuf/message.h>

#include <a4/a4io.h>

namespace google{ namespace protobuf{ namespace io{ class CodedInputStream; };};};

namespace a4{ namespace io{

    namespace internal {

        // Wizardry to get class from ID
        typedef shared<Message> (*from_stream_func)(google::protobuf::io::CodedInputStream *);
        from_stream_func all_class_ids(int, from_stream_func f = NULL);

        template <typename ProtoClass>
        shared<Message> from_stream(google::protobuf::io::CodedInputStream * instr) {
            shared<ProtoClass> msg(new ProtoClass());
            msg->ParseFromCodedStream(instr);
            return msg;
        }

        template <typename ProtoClass>
        int reg_protoclass_id() {
            if (ProtoClass::kCLASSIDFieldNumber != 0) {
                all_class_ids(ProtoClass::kCLASSIDFieldNumber, from_stream<ProtoClass>);
            }
            return ProtoClass::kCLASSIDFieldNumber;
        }
    }

    template <typename ProtoClass>
    class RegisterClassID {
        public:
            static int class_id;
            virtual uint32_t get_class_id() { return class_id; } // forces class_id to be inited
    };

    template <typename ProtoClass>
    int RegisterClassID<ProtoClass>::class_id = internal::reg_protoclass_id<ProtoClass>();

    /// Class indicating that no MetaData is being used
    class NoProtoClass : public Message {
        public:
            NoProtoClass() {};
            virtual Message* New() const {return NULL;};
            virtual int GetCachedSize() const {return 0;};
            virtual google::protobuf::Metadata GetMetadata() const { return google::protobuf::Metadata(); };
            static const int kCLASSIDFieldNumber = 0;
            void ParseFromCodedStream(google::protobuf::io::CodedInputStream *) {};
    };

};};

#define A4RegisterClass(X) template class a4::io::RegisterClassID<X>;

#endif
