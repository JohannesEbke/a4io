#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>

#include <a4/message.h>

#include <A4.pb.h>

#include "dynamic_message.h"

using google::protobuf::DynamicMessageFactory;
using google::protobuf::Descriptor;
using google::protobuf::DescriptorPool;
using google::protobuf::Message;

namespace a4{ namespace io{

    A4Message::~A4Message() {
        message.reset();
        _factory.reset();
        _pool.reset();
    }

    void A4Message::version_check(const A4Message &m2) const {
        if (descriptor()->full_name() != m2.descriptor()->full_name()) {
            throw a4::Fatal("Typenames of objects to merge do not agree: ", descriptor()->full_name(), " != ", m2.descriptor()->full_name());
        }
        const Descriptor * d1;
        if (_dynamic_descriptor) d1 = _dynamic_descriptor;
        else d1 = _pool->FindMessageTypeByName(descriptor()->full_name());
        
        const Descriptor * d2;
        if (m2._dynamic_descriptor) d2 = m2._dynamic_descriptor;
        else d2 = m2._pool->FindMessageTypeByName(m2.descriptor()->full_name());

        assert(d1);
        assert(d2);

        if (d1 == d2) return;

        // Do version checking if the dynamic descriptors are different
        std::string mymajor = d1->options().GetExtension(major_version);
        std::string myminor = d1->options().GetExtension(minor_version);
        std::string dmajor = d2->options().GetExtension(major_version);
        std::string dminor = d2->options().GetExtension(minor_version);

        if (mymajor != dmajor) {
            throw a4::Fatal("Major versions of objects to merge do not agree:", mymajor, " != ", dmajor);
        } else if (myminor != dminor) {
            std::cerr << "Warning: Minor versions of merged messages do not agree:" << myminor << " != " << dminor << std::endl;
        }
    }

    A4Message A4Message::operator+(const A4Message & m2_) const {
        // Find out which descriptor to use. Prefer dynamic descriptors
        // since they are probably contain all fields.
        version_check(m2_);

        const Descriptor * d;
        if (m2_._dynamic_descriptor) d = _dynamic_descriptor;
        else d = m2_._pool->FindMessageTypeByName(m2_.descriptor()->full_name());

        // Prepare dynamic messages
        A4Message res, m1, m2;
        res = m2_;
        res._dynamic_descriptor = d;
        m1 = m2 = res;

        res.message.reset(m2._factory->GetPrototype(d)->New());
        if (_dynamic_descriptor == d) {
            m1.message = this->message;
        } else {
            m1.message.reset(res.message->New());
            m1.message->ParseFromString(message->SerializeAsString());
        }

        if (m2_._dynamic_descriptor == d) {
            m2.message = m2_.message;
        } else {
            m2.message.reset(res.message->New());
            m2.message->ParseFromString(m2_.message->SerializeAsString());
        }

        for (int i = 0; i < d->field_count(); i++) {
            MetadataMergeOptions merge_opts = d->field(i)->options().GetExtension(merge);

            DynamicField f1(*m1.message, d->field(i));
            DynamicField f2(*m2.message, d->field(i));
            DynamicField fm(*res.message, d->field(i));

            switch(merge_opts) {
                case MERGE_BLOCK_IF_DIFFERENT:
                    if(!(f1 == f2)) throw a4::Fatal("Trying to merge metadata objects with different entries in ", f1.name());
                    fm.set(f1.value());
                    break;
                case MERGE_ADD:
                    add_fields(f1, f2, fm);
                    break;
                case MERGE_MULTIPLY:
                    multiply_fields(f1, f2, fm);
                    break;
                case MERGE_UNION:
                    append_fields(f1, f2, fm, true);
                    break;
                case MERGE_APPEND:
                    append_fields(f1, f2, fm, false);
                    break;
                case MERGE_DROP:
                    break;
                default:
                    throw a4::Fatal("Unknown merge strategy: ", merge_opts, ". Recompilation should fix it.");
            }
        }
        return res;
    }
    
    std::string A4Message::field_as_string(const std::string & field_name) {
        assert(descriptor() == message->GetDescriptor());
        const FieldDescriptor* fd = descriptor()->FindFieldByName(field_name);
        DynamicField f(*message, fd);
        if (f.repeated()) {
            std::stringstream ss;
            for (int i = 0; i < f.size(); i++) ss << f.value(i).str();
            return ss.str();
        } else {
            return f.value().str();
        }
    }

    std::string A4Message::assert_field_is_single_value(const std::string & field_name) {
        assert(descriptor() == message->GetDescriptor());
        const FieldDescriptor* fd = descriptor()->FindFieldByName(field_name);
        if (!fd) {
            const std::string & classname = message->GetDescriptor()->full_name();
            throw a4::Fatal(classname, " has no member ", field_name, " necessary for metadata merging or splitting!");
        }
        if (fd->is_repeated() && (message->GetReflection()->FieldSize(*message, fd)) > 1) {
            throw a4::Fatal(fd->full_name(), " has already multiple ", field_name, " entries - cannot achieve desired granularity!");
        }
        return field_as_string(field_name);
    }

};};
