// MIT License - see LICENSE
//
// Overview page about Nt Protobuf for the NDoc documentation. This file holds only a documentation comment and is not
// part of the library.

/*!
    \page nt-protobuf.html
    \title Using Nt Protobuf
    \brief Protocol Buffers messages as C++ classes, generated from .proto files, in binary and JSON.

    Nt Protobuf is a subset of Qt Protobuf. \c ntprotobufgen reads \c .proto files, proto2 or proto3,
    and writes a C++ class for each message; QProtobufSerializer writes and reads them in the binary
    format of Protocol Buffers, which every other implementation reads, and QProtobufJsonSerializer
    in its JSON mapping. The generator has a parser of its own: building needs neither \c protoc nor
    libprotobuf.

    \section1 Building

    \c qt_add_protobuf() runs the generator on the \c .proto files and builds the classes into a
    library, or into an existing target:

    \code
    find_package(Nt6 REQUIRED COMPONENTS Protobuf)   # or find_package(Qt6 COMPONENTS Protobuf ProtobufTools)

    qt_add_protobuf(messages
        PROTO_FILES proto/tutorial/addressbook.proto
        PROTO_INCLUDES proto)
    target_link_libraries(app PRIVATE messages)
    \endcode

    Each \c file.proto gives \c file.qpb.h and \c file.qpb.cpp, at its path under the first
    \c PROTO_INCLUDES directory that holds it: the file above is included as
    \c{"tutorial/addressbook.qpb.h"}. Imports are looked up in \c PROTO_INCLUDES and next to the
    importing file. \c nt_add_protobuf() is the same function under the name of Nt. The generator
    can also be run by hand:

    \code
    ntprotobufgen -I proto --output-dir generated proto/tutorial/addressbook.proto
    \endcode

    \section1 The Generated Classes

    For

    \code
    syntax = "proto3";
    package tutorial;

    message Person {
      string name = 1;
      int32 id = 2;
      optional string email = 3;
      enum PhoneType { MOBILE = 0; HOME = 1; WORK = 2; }
      message PhoneNumber { string number = 1; PhoneType type = 2; }
      repeated PhoneNumber phones = 4;
      map<string, int32> scores = 5;
      oneof contact { string phone = 6; string fax = 7; }
    }
    \endcode

    the generator writes the class \c tutorial::Person, a QProtobufMessage:

    \list
        \li A package becomes namespaces: \c tutorial::Person. A nested type is in the namespace
            \c Person_QtProtobufNested and also named inside the class, as \c Person::PhoneNumber.
        \li Each field has a getter and a setter named after it in lowerCamelCase: \c name() and
            \c setName(), \c id() and \c setId(). The scalar types are those of the QtProtobuf
            namespace, strings are QString and bytes QByteArray.
        \li A message, list or map field is returned by \c const reference, and \c mut<Field>()
            returns it for changing in place: \c person.mutPhones().append(number).
        \li Lists are QList, of the QtProtobuf scalar types (\c QtProtobuf::int32List) or of
            messages (\c Person::PhoneNumberRepeated), and maps are QHash.
        \li An enum is an \c{enum class} with the values of the \c .proto file.
        \li A field with presence (a message, \c optional in proto3, any singular field in proto2)
            has \c has<Field>() and \c clear<Field>(). A field of a oneof has \c has<Field>(), and the
            oneof has \c contactField(), returning a \c ContactFields value (\c UninitializedField,
            \c Phone or \c Fax), and \c clearContact(); setting one field of a oneof clears the
            others.
        \li \c Person::NameProtoFieldNumber and the like give the field numbers.
        \li Messages are copied cheaply: a copy shares the data until one of them changes.
    \endlist

    \code
    tutorial::Person person;
    person.setName(QStringLiteral("Ada"));
    person.setId(1815);
    tutorial::Person::PhoneNumber phone;
    phone.setNumber(QStringLiteral("555-0100"));
    phone.setType(tutorial::Person::PhoneType::HOME);
    person.mutPhones().append(phone);
    person.mutScores().insert(QStringLiteral("chess"), 1200);

    QProtobufSerializer serializer;
    const QByteArray bytes = person.serialize(&serializer);

    tutorial::Person read;
    if (!read.deserialize(&serializer, bytes))
        qWarning() << serializer.lastErrorString();
    \endcode

    \section1 Binary and JSON

    QProtobufSerializer writes what \c protoc writes: scalars that hold their default value are left
    out, numeric lists of proto3 are packed, fields are in the order of their numbers, and fields that
    the class does not know are kept and written back. The tests of Nt check the bytes against
    \c protoc in both directions.

    QProtobufJsonSerializer writes the JSON mapping of proto3: lowerCamelCase names (or
    \c json_name), 64-bit integers as strings, bytes in base64, enum values by name and maps as
    objects.

    \section1 What Is Missing

    gRPC services and the generator's \c QML option are not supported, and the well-known types
    (\c google/protobuf/timestamp.proto and the others) are not shipped yet, so a file that imports
    them needs them on its include path, and they have no special JSON form. Extensions, groups and
    editions are not supported. The naming of the generated API follows Qt's documentation where it
    shows it; the forms it does not show (the namespaces of nested types, the oneof and presence
    functions) follow the conventions of Qt Protobuf as far as they are known, and may differ in
    details.

    \sa {Nt Protobuf C++ Classes}
*/
