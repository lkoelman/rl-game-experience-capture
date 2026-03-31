protoc = find_program('protoc')
proto_gen = generator(
  protoc,
  output : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
  arguments : [
    '--proto_path=@CURRENT_SOURCE_DIR@',
    '--cpp_out=@BUILD_DIR@',
    '@INPUT@',
  ],
)

generated_protos = proto_gen.process('gamepad.proto')

