file(GLOB ProtoFiles
        "*.proto"
        "framework/*.proto"
        )

PROTOBUF_GENERATE_CPP(ProtobufSources ProtobufHeaders ${ProtoFiles})

# PROTOBUF_GENERATE_CPP has limitation and can handle proto files only one single directory
# To work-around, hard-code generated file paths.
list(APPEND SRCS
        ${CMAKE_BINARY_DIR}/protos/diagnostics.pb.cc
        ${CMAKE_BINARY_DIR}/protos/framework/request.pb.cc
        ${CMAKE_BINARY_DIR}/protos/framework/response.pb.cc
        )

set(GENERATED_PROTO_SRCS ${SRCS} PARENT_SCOPE)

add_custom_target(build_protos DEPENDS ${ProtobufSources})
