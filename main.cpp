#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <io.h>
#include <strstream>

#include <filesystem>
#include <png.h>

#include "EasyGifReader.h"



// #ifndef DEBUG
// #define DEBUG
// #endif

using namespace std;

png_bytepp frameToPngBytePP(const EasyGifReader::Frame& frame);

png_bytepp allocateRowBytePP(const EasyGifReader& reader);

void setFrameToBytePP(const png_bytepp& inputArray, const EasyGifReader::Frame& frame, bool transparent = false);

void writeRowsToPNG(const png_bytepp& inputArray, const EasyGifReader::Frame& frame, png_structp writeTarget);

void deallocateRowBytePP(const png_bytepp& rowBuffer, const EasyGifReader& reader);

int main(int argc, char** argv) {
    // std::cout << "Hello, World!" << std::endl;

    if (argc < 7) {
        std::cout << "Make sure to include -i INPUT_FILE.gif, -o OUTPUT_TEXTURE_ID, and -d OUTPUT_DIRECTORY!" << std::endl;
    }

    vector<string> arguments;
    for (int i = 1; i < argc; i++) {
        arguments.emplace_back(argv[i]);
    }

    //required parameters
    string input_gif;
    string output_id;
    string output_directory;

    //optional parameters
    string test_type;
    bool interpolate = false;


    for (int i = 0; i < arguments.size(); i++) {
        if (arguments[i] == "-i") {
            input_gif = arguments[i + 1];
        }

        if (arguments[i] == "-o") {
            output_id = arguments[i + 1];
        }

        if (arguments[i] == "-d") {
            output_directory = arguments[i + 1];
        }

        if (arguments.size() <= 5) {
            continue;//don't process debug flags
        }

        //valid test types: chunkedFF, rowedFF, firstFrame
        if (arguments[i] == "-tt") {
            test_type = arguments[i + 1];
        }

        //interpolate between frames for framerates greater than 1 tick per frame
        if (arguments[i] == "-interp") {
            interpolate = true;
        }
    }

    //take the input from the file
    ifstream input_stream(input_gif);

    if (!input_stream.good()) {
        return -1;
    }

    input_stream.close();

    filesystem::__cxx11::path outputPath = filesystem::__cxx11::path(output_directory);

    filesystem::__cxx11::path output_location = outputPath / (output_id + ".png");





    EasyGifReader gif = EasyGifReader::openFile(input_gif.c_str());


    int output_height = gif.height()/* * gif.frameCount() */;

    if (test_type.empty()) {
        output_height *= gif.frameCount();
    }

    const int output_width = gif.width();


    png_struct* output_struct = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_info* output_info = png_create_info_struct(output_struct);

    if (setjmp(png_jmpbuf(output_struct))) {//I know, I have to do this because C
        cout << "setjump failed?" << endl;
    }



    // png_FILE_p output_file = fopen64(reinterpret_cast<const char*>(output_location.c_str()), "wb");

    png_FILE_p output_file = fopen64(output_location.string().c_str(), "wb");


    png_init_io(output_struct, output_file);
    png_set_IHDR(
        output_struct, //png struct
        output_info, //info struct
        output_width, //width
        output_height, //height
        8, //bit depth
        PNG_COLOR_TYPE_RGBA, //color type
        PNG_INTERLACE_NONE, //interlace type
        PNG_COMPRESSION_TYPE_DEFAULT, //compression type
        PNG_FILTER_TYPE_DEFAULT //filter type
        );

    png_write_info(output_struct, output_info); //write png image information to file

    png_bytepp first_frame = nullptr;

    //allocate and fill the first frame if we are in a test
    if (!test_type.empty()) {
        first_frame = frameToPngBytePP(*gif.begin());
    }

    //output the entire first frame
    if (test_type == "firstFrame") {
        png_write_image(output_struct, first_frame);
    }

    //yeah, I'm not good enough to handle that kind of responsibility lol
    // if (test_type == "chunkedFF") {
    //     png_write_chunk_start(output_struct, "IDAT", 0x2000);
    // }

    //test outputting single rows at a time
    if (test_type == "rowedFF") {
        for (int i = 0; i < gif.begin()->height(); i++) {
            png_write_row(output_struct, first_frame[i]);
        }
    }

    //make sure we are not doing a test
    if (test_type.empty()) {

        //allocate a buffer for rows
        png_bytepp rowBuffer = allocateRowBytePP(gif);

        //DEBUG: keep track of the frame
#ifdef DEBUG
        int frameIndex = 0;
#endif
        //iterate over the frames in the gif
        for (const EasyGifReader::Frame& frame : gif) {
            //fill the row buffer for the current frame
            setFrameToBytePP(rowBuffer, frame);
            //write the current frame to the output png
            writeRowsToPNG(rowBuffer, frame, output_struct);

            //DEBUG: output the current frame
#ifdef DEBUG
            frameIndex++;
            cout << "frame: " << frameIndex << " written" << endl;
#endif

        }
        //deallocate the row buffer
        deallocateRowBytePP(rowBuffer, gif);

    }





    png_write_end(output_struct, NULL);//close the file

    //deallocate the test frame if it exists
    if (first_frame != nullptr) {
        for (int i = 0; i < gif.begin()->height(); i++) {
            delete [] first_frame[i];
        }

        delete [] first_frame;
    }

    //build the animation JSON file
    strstream json_animation_output{};

    json_animation_output << "{" << endl;
    json_animation_output << "\t" << R"("__comment": ")" << "Generated texture.png and texture.png.mcmeta from gif using png_stuff by gamma_02. Texture ID: " << output_id << "\"," << endl;
    json_animation_output << "\t" << R"("animation": {)" << endl;
    if (interpolate) {
        json_animation_output << "\t\t" << R"("interpolate": true,)" << endl;
    }
    json_animation_output << "\t\t" << R"("width": )" << gif.width() << "," << endl;
    json_animation_output << "\t\t" << R"("height": )" << gif.height() << "," << endl;
    int default_frametime = static_cast<int>(ceil(gif.begin()->duration().milliseconds() / 50.0));
#ifdef DEBUG
    cout << "DEFAULT FRAMETIME: " << default_frametime << endl;
    cout << gif.begin()->duration().milliseconds() / 50.0 << endl;
#endif
    json_animation_output << "\t\t" << R"("frametime": )" << default_frametime << "," << endl;
    json_animation_output << "\t\t" << R"("frames": [)";
    int frameIndex = 0;
    for (const EasyGifReader::Frame& frame : gif) {

        int frame_frametime = static_cast<int>(ceil(frame.duration().milliseconds() / 50.0));

        if (frame_frametime != default_frametime) {
            json_animation_output << R"({"index": )" << frameIndex << R"(, "time": )" << frame_frametime << "}, ";
        } else if (frameIndex + 1 != gif.frameCount()) {
            json_animation_output << frameIndex << ", ";
        } else if (frameIndex + 1 == gif.frameCount()) {
            json_animation_output << frameIndex;
        }

        frameIndex += 1;
    }
    json_animation_output << "]" << endl;
    json_animation_output << "\t}" << endl;
    json_animation_output << "}" << endl;

#ifdef DEBUG
    cout << "Finished writing png, creating JSON: " << endl;
    cout << json_animation_output.str() << endl;
#endif

    ofstream outputStream;

    filesystem::__cxx11::path jsonOutputPath = outputPath / ( output_id + ".png.mcmeta");

    outputStream.open(jsonOutputPath);

    outputStream << json_animation_output.str();

    outputStream.close();

    return 0;
}

png_bytepp frameToPngBytePP(const EasyGifReader::Frame& frame) {
    auto output = new png_byte*[frame.height()];
    auto pixels = frame.pixels();

    int framePixelIndex = 0;
    for (int y = 0; y < frame.height(); y++) {
        output[y] = new png_byte[4 * frame.width()];
        for (int x = 0; x < 4 * frame.width(); x += 4) {
            output[y][x + 0] = pixels[framePixelIndex + 0];//red
            output[y][x + 1] = pixels[framePixelIndex + 1];//green
            output[y][x + 2] = pixels[framePixelIndex + 2];//blue
            output[y][x + 3] = pixels[framePixelIndex + 4];//alpha

            framePixelIndex += 4;
        }

    }



    return output;
}

png_bytepp allocateRowBytePP(const EasyGifReader& reader) {
    auto output = new png_byte*[reader.height()];

    for (int y = 0; y < reader.height(); y++) {
        output[y] = new png_byte[reader.width()*4];
    }

    return output;


}

void deallocateRowBytePP(const png_bytepp& rowBuffer, const EasyGifReader& reader) {
    for (int y = 0; y < reader.height(); y++) {
        delete [] rowBuffer[y];
    }

    delete [] rowBuffer;
}

void setFrameToBytePP(const png_bytepp& inputArray, const EasyGifReader::Frame& frame, bool transparent) {
    auto pixels = frame.pixels();

    int framePixelIndex = 0;
    for (int y = 0; y < frame.height(); y++) {
        for (int x = 0; x < 4 * frame.width(); x += 4) {
            inputArray[y][x + 0] = pixels[framePixelIndex + 0];//red
            inputArray[y][x + 1] = pixels[framePixelIndex + 1];//green
            inputArray[y][x + 2] = pixels[framePixelIndex + 2];//blue
            inputArray[y][x + 3] = pixels[transparent ? framePixelIndex + 4 : 0xFF];//alpha

            framePixelIndex += 4;
        }
    }

}

//this is a stupid small amount of code. it scares me.
void writeRowsToPNG(const png_bytepp& inputArray, const EasyGifReader::Frame& frame, png_structp writeTarget) {

    for (int i = 0; i < frame.height(); i++) {
        png_write_row(writeTarget, inputArray[i]);
    }

}



