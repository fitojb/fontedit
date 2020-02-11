#ifndef FORMAT_H
#define FORMAT_H

#include <string>
#include <iostream>
#include <iomanip>
#include "sourcecode.h"

namespace Format
{

using namespace SourceCode;

struct C
{
    static constexpr std::string_view identifier = "c";

    template<Idiom I>
    static std::ostream& append(std::ostream& o, const Element::Bare<I>&) {

        switch (I) {
        case IdiomBeginArrayRow:
            o << "\t";
            break;
        case IdiomLineBreak:
            o << "\n";
            break;
        case IdiomEndArray:
            o << "};\n";
            break;
        case IdiomEnd:
            o << "\n\n";
            break;
        default:
            break;
        }

        return o;
    }

    template<Idiom I>
    static std::ostream& append(std::ostream& o, const Element::String<I>& e) {
        switch (I) {
        case IdiomBegin:
            o << "//\n// Font Data\n// Created: " << e.arg << "\n//\n";
            break;
        case IdiomBeginArray:
            o << "\n\nconst unsigned char " << e.arg << "[] = {\n";
            break;
        case IdiomComment:
            o << " // " << e.arg;
            break;
        default:
            break;
        }

        return o;
    }

    template<Idiom I>
    static std::ostream& append(std::ostream& o, const Element::Byte<I>& e) {
        o << "0x"
          << std::setw(2)
          << std::setfill('0')
          << std::uppercase
          << std::hex
          << static_cast<uint>(e.byte)
          << ",";

        return o;
    }
};

struct Arduino
{
    static constexpr std::string_view identifier = "arduino";

    template<Idiom I>
    static std::ostream& append(std::ostream& o, const Element::Byte<I>& e) {
        return C::append(o, e);
    }

    template<Idiom I>
    static std::ostream& append(std::ostream& o, const Element::Bare<I>& e) {
        return C::append(o, e);
    }

    template<Idiom I>
    static std::ostream& append(std::ostream& o, const Element::String<I>& e)
    {
        switch (I) {
        case IdiomBegin:
            C::append(o, e);
            o << "\n#include <Arduino.h>\n";
            break;
        case IdiomBeginArray:
            o << "\n\nconst uint8_t " << e.arg << "[] PROGMEM = {\n";
            break;
        default:
            C::append(o, e);
            break;
        }

        return o;
    }
};

}

#endif // FORMAT_H
