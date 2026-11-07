"""Prepare the exact pre-change processor for the same Windows callback harness."""
import pathlib, subprocess, tarfile, io
root=pathlib.Path(__file__).resolve().parents[1]
destination=root/'benchmark-baseline-source'
revision='b6851c0f7a8f86a451f694b762e52d539e49c94f'
subprocess.run(['git','fetch','--depth=1','origin',revision],check=True)
data=subprocess.check_output(['git','archive',revision,'CMakeLists.txt','Source','cmake','prototype','Resources'])
destination.mkdir(exist_ok=True)
with tarfile.open(fileobj=io.BytesIO(data)) as archive:
    archive.extractall(destination,filter='data')
(destination/'tests').mkdir(exist_ok=True)
# The unchanged CMake file references test sources; they are not baseline audio code.
for file in (root/'tests').glob('*'):
    if file.is_file(): (destination/'tests'/file.name).write_bytes(file.read_bytes())
fixtures=destination/'tests/fixtures';fixtures.mkdir(exist_ok=True)
for file in (root/'tests/fixtures').glob('*'):
    if file.is_file(): (fixtures/file.name).write_bytes(file.read_bytes())
with (destination/'CMakeLists.txt').open('a') as out:
    out.write('''\nadd_executable(WubrotCallbackBenchmark tests/CallbackBenchmark.cpp)
target_include_directories(WubrotCallbackBenchmark PRIVATE Source ${juce_SOURCE_DIR}/modules)
target_compile_definitions(WubrotCallbackBenchmark PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
target_link_libraries(WubrotCallbackBenchmark PRIVATE WUBROT)
if(WIN32)
    target_link_libraries(WubrotCallbackBenchmark PRIVATE psapi)
endif()
''')
print('Prepared baseline processor at',revision)
