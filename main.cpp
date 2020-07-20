#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string>
#include <filesystem>

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef char u8;

struct GXIn
{
    u8 texture_type;
    u16 width;
    u16 height;
    u32 file_size;
    u32 texel_count;
    u8 texel_size_rgb5a3 = 2; //2 bytes
    u16* texel_data_rgb5a3;
    u8 chunk_size_cmp = 32; //32 bytes
    u32 chunk_count_cmp;
    u8* texel_data_cmp;
    u8 tile_row_size;
    u8 tile_column_size;
    u32* ordered_texel_data;
};

void invert_endianness(char* read_data, u32 bytes)
{
    char* temp = new char[bytes];
    for (u32 i = 0; i < bytes; i++)
    {
        temp[i] = read_data[i];
    }

    u8 temp_index = bytes - 1;
    for (u32 j = 0; j < bytes; j++)
    {
        read_data[j] = temp[temp_index];
        temp_index--;
    }

    delete[] temp;
}

u32 processFile(std::fstream* in_file, std::fstream* out_file, std::fstream* log_file)
{
    GXIn in_tex;
    u8 read_size = 4;
    char* read_data = new char[read_size];

    char* log_temp = new char[100];
    u16 to_write = 0;

    in_file->seekg(0x00, std::iostream::beg);
    in_file->read(read_data, read_size);
    in_tex.texture_type = (u8)(read_data[3] & 0xff);
    to_write = sprintf(log_temp, "Texture Type: %.2X\n", in_tex.texture_type);
    log_file->write(log_temp, to_write);

    in_file->read(read_data, read_size);
    in_tex.width = (u16)(((read_data[2] & 0xff) << 8) | (read_data[3] & 0xff));
    to_write = sprintf(log_temp, "Width: %.4X\n", in_tex.width);
    log_file->write(log_temp, to_write);

    in_file->read(read_data, read_size);
    in_tex.height = (u16)(((read_data[2] & 0xff) << 8) | (read_data[3] & 0xff));
    to_write = sprintf(log_temp, "Height: %.4X\n", in_tex.height);
    log_file->write(log_temp, to_write);

    in_tex.texel_count = in_tex.height * in_tex.width;
    to_write = sprintf(log_temp, "Texel Count: %.8X\n", in_tex.texel_count);
    log_file->write(log_temp, to_write);

    delete[] read_data;
    delete[] log_temp;

    if (in_tex.texture_type == 0)
    {
        in_file->seekg(0x20, std::iostream::beg);
        char* read_texel = new char[in_tex.texel_size_rgb5a3];
        in_tex.texel_data_rgb5a3 = new u16[in_tex.texel_count];
        for(u32 curr_tex = 0; curr_tex < in_tex.texel_count; curr_tex++)
        {
            in_file->read(read_texel, in_tex.texel_size_rgb5a3);
            in_tex.texel_data_rgb5a3[curr_tex] = (u16)(((read_texel[0] & 0xff) << 8) | (read_texel[1] & 0xff));
        }
        in_file->close();

        delete[] read_texel;
    }
    else if (in_tex.texture_type == 2)
    {
        in_file->seekg(0x00, std::iostream::end);
        u32 in_size = in_file->tellg();
        in_size -= 0x20; //Remove header
        in_tex.chunk_count_cmp = (u32)in_size / in_tex.chunk_size_cmp;

        char* read_byte = new char[1];
        in_tex.texel_data_cmp = new char[in_size];

        in_file->seekg(0x20, std::iostream::beg);
        for (u32 curr = 0; curr < in_size; curr++)
        {
            in_file->read(read_byte, 1);
            in_tex.texel_data_cmp[curr] = (u8)read_byte[0];
        }
        in_file->close();

        delete[] read_byte;
    }
    else
    {
        std::cout << "Unrecognized Texture Type" << std::endl;
        return 1;
    }

    char* bitmap_header = new char[14 + 118]; //HEADER SIZE, Everything is LE, BITMAPV4HEADER

    bitmap_header[0x00] = 'B';
    bitmap_header[0x01] = 'M';  //Identifying type

    bitmap_header[0x06] = 0x00;
    bitmap_header[0x07] = 0x00;
    bitmap_header[0x08] = 0x00;
    bitmap_header[0x09] = 0x00; //Reserved

    u32 bitmap_data_offset = 122;
    bitmap_header[0x0A] = (u8)(bitmap_data_offset & 0x000000ff);
    bitmap_header[0x0B] = (u8)((bitmap_data_offset & 0x0000ff00) >> 8);
    bitmap_header[0x0C] = (u8)((bitmap_data_offset & 0x00ff0000) >> 16);
    bitmap_header[0x0D] = (u8)((bitmap_data_offset & 0xff000000) >> 24);

    u32 size_of_bitmap_header = 108; //BITMAPV4HEADER
    bitmap_header[0x0E] = (u8)(size_of_bitmap_header & 0x000000ff);
    bitmap_header[0x0F] = (u8)((size_of_bitmap_header & 0x0000ff00) >> 8);
    bitmap_header[0x10] = (u8)((size_of_bitmap_header & 0x00ff0000) >> 16);
    bitmap_header[0x11] = (u8)((size_of_bitmap_header & 0xff000000) >> 24);

    bitmap_header[0x12] = (u8)(in_tex.width & 0x000000ff);
    bitmap_header[0x13] = (u8)((in_tex.width & 0x0000ff00) >> 8);
    bitmap_header[0x14] = (u8)((in_tex.width & 0x00ff0000) >> 16);
    bitmap_header[0x15] = (u8)((in_tex.width & 0xff000000) >> 24);

    bitmap_header[0x16] = (u8)(in_tex.height & 0x000000ff);
    bitmap_header[0x17] = (u8)((in_tex.height & 0x0000ff00) >> 8);
    bitmap_header[0x18] = (u8)((in_tex.height & 0x00ff0000) >> 16);
    bitmap_header[0x19] = (u8)((in_tex.height & 0xff000000) >> 24);

    u16 colour_plane_count = 1;
    bitmap_header[0x1A] = (u8)(colour_plane_count & 0x00ff);
    bitmap_header[0x1B] = (u8)((colour_plane_count & 0xff00) >> 8);
    u16 bpp = 32;
    bitmap_header[0x1C] = (u8)(bpp & 0x00ff);
    bitmap_header[0x1D] = (u8)((bpp & 0xff00) >> 8);

    u32 compression_method = 3; //BI_BITFIELDS
    bitmap_header[0x1E] = (u8)(compression_method & 0x000000ff);
    bitmap_header[0x1F] = (u8)((compression_method & 0x0000ff00) >> 8);
    bitmap_header[0x20] = (u8)((compression_method & 0x00ff0000) >> 16);
    bitmap_header[0x21] = (u8)((compression_method & 0xff000000) >> 24);

    u32 resolution = 3780;
    bitmap_header[0x26] = (u8)(resolution & 0x000000ff); //Horiz Resolution
    bitmap_header[0x27] = (u8)((resolution & 0x0000ff00) >> 8);
    bitmap_header[0x28] = (u8)((resolution & 0x00ff0000) >> 16);
    bitmap_header[0x29] = (u8)((resolution & 0xff000000) >> 24);

    bitmap_header[0x2A] = (u8)(resolution & 0x000000ff); //Vert Resolution
    bitmap_header[0x2B] = (u8)((resolution & 0x0000ff00) >> 8);
    bitmap_header[0x2C] = (u8)((resolution & 0x00ff0000) >> 16);
    bitmap_header[0x2D] = (u8)((resolution & 0xff000000) >> 24);

    bitmap_header[0x2E] = 0x00; //Colours Used
    bitmap_header[0x2F] = 0x00;
    bitmap_header[0x30] = 0x00;
    bitmap_header[0x31] = 0x00;

    bitmap_header[0x32] = 0x00; //Num of Important Colours
    bitmap_header[0x33] = 0x00;
    bitmap_header[0x34] = 0x00;
    bitmap_header[0x35] = 0x00;

    u32 red_mask = 0xFF000000; //Red Mask
    bitmap_header[0x36] = (u8)(red_mask & 0x000000ff);
    bitmap_header[0x37] = (u8)((red_mask & 0x0000ff00) >> 8);
    bitmap_header[0x38] = (u8)((red_mask & 0x00ff0000) >> 16);
    bitmap_header[0x39] = (u8)((red_mask & 0xff000000) >> 24);

    u32 green_mask = 0x00FF0000; //Green Mask
    bitmap_header[0x3A] = (u8)(green_mask & 0x000000ff);
    bitmap_header[0x3B] = (u8)((green_mask & 0x0000ff00) >> 8);
    bitmap_header[0x3C] = (u8)((green_mask & 0x00ff0000) >> 16);
    bitmap_header[0x3D] = (u8)((green_mask & 0xff000000) >> 24);

    u32 blue_mask = 0x0000FF00; //Blue Mask
    bitmap_header[0x3E] = (u8)(blue_mask & 0x000000ff);
    bitmap_header[0x3F] = (u8)((blue_mask & 0x0000ff00) >> 8);
    bitmap_header[0x40] = (u8)((blue_mask & 0x00ff0000) >> 16);
    bitmap_header[0x41] = (u8)((blue_mask & 0xff000000) >> 24);

    u32 alpha_mask = 0x000000FF; //Alpha Mask
    bitmap_header[0x42] = (u8)(alpha_mask & 0x000000ff);
    bitmap_header[0x43] = (u8)((alpha_mask & 0x0000ff00) >> 8);
    bitmap_header[0x44] = (u8)((alpha_mask & 0x00ff0000) >> 16);
    bitmap_header[0x45] = (u8)((alpha_mask & 0xff000000) >> 24);

    u32 colour_space_type = 0x73524742; //Device sRGB (Standard)
    bitmap_header[0x46] = (u8)(colour_space_type & 0x000000ff);
    bitmap_header[0x47] = (u8)((colour_space_type & 0x0000ff00) >> 8);
    bitmap_header[0x48] = (u8)((colour_space_type & 0x00ff0000) >> 16);
    bitmap_header[0x49] = (u8)((colour_space_type & 0xff000000) >> 24);

    //CIEXYZ Endpoints are all 0, 36 bytes
    bitmap_header[0x4A] = 0x00;
    bitmap_header[0x4B] = 0x00;
    bitmap_header[0x4C] = 0x00;
    bitmap_header[0x4D] = 0x00;
    bitmap_header[0x4E] = 0x00;
    bitmap_header[0x4F] = 0x00;
    bitmap_header[0x50] = 0x00;
    bitmap_header[0x51] = 0x00;
    bitmap_header[0x52] = 0x00;
    bitmap_header[0x53] = 0x00;
    bitmap_header[0x54] = 0x00;
    bitmap_header[0x55] = 0x00;
    bitmap_header[0x56] = 0x00;
    bitmap_header[0x57] = 0x00;
    bitmap_header[0x58] = 0x00;
    bitmap_header[0x59] = 0x00;
    bitmap_header[0x5A] = 0x00;
    bitmap_header[0x5B] = 0x00;
    bitmap_header[0x5C] = 0x00;
    bitmap_header[0x5D] = 0x00;
    bitmap_header[0x5E] = 0x00;
    bitmap_header[0x5F] = 0x00;
    bitmap_header[0x60] = 0x00;
    bitmap_header[0x61] = 0x00;
    bitmap_header[0x62] = 0x00;
    bitmap_header[0x63] = 0x00;
    bitmap_header[0x64] = 0x00;
    bitmap_header[0x65] = 0x00;
    bitmap_header[0x66] = 0x00;
    bitmap_header[0x67] = 0x00;
    bitmap_header[0x68] = 0x00;
    bitmap_header[0x69] = 0x00;
    bitmap_header[0x6A] = 0x00;
    bitmap_header[0x6B] = 0x00;
    bitmap_header[0x6C] = 0x00;
    bitmap_header[0x6D] = 0x00;

    bitmap_header[0x6E] = 0x00; //Gamma Red
    bitmap_header[0x6F] = 0x00;
    bitmap_header[0x70] = 0x00;
    bitmap_header[0x71] = 0x00;

    bitmap_header[0x72] = 0x00; //Gamma Green
    bitmap_header[0x73] = 0x00;
    bitmap_header[0x74] = 0x00;
    bitmap_header[0x75] = 0x00;

    bitmap_header[0x76] = 0x00; //Gamma Blue
    bitmap_header[0x77] = 0x00;
    bitmap_header[0x78] = 0x00;
    bitmap_header[0x79] = 0x00;

    u32 row_size = in_tex.width * 4; //Number of bytes in a row of the image

    u32 raw_image_size = (u32)((row_size) * in_tex.height); //Raw Image Size
    bitmap_header[0x22] = (u8)(raw_image_size & 0x000000ff);
    bitmap_header[0x23] = (u8)((raw_image_size & 0x0000ff00) >> 8);
    bitmap_header[0x24] = (u8)((raw_image_size & 0x00ff0000) >> 16);
    bitmap_header[0x25] = (u8)((raw_image_size & 0xff000000) >> 24);

    u32 bmp_size = (u32)raw_image_size + size_of_bitmap_header + 14;
    bitmap_header[0x02] = (u8)(bmp_size & 0x000000ff);
    bitmap_header[0x03] = (u8)((bmp_size & 0x0000ff00) >> 8);
    bitmap_header[0x04] = (u8)((bmp_size & 0x00ff0000) >> 16);
    bitmap_header[0x05] = (u8)((bmp_size & 0xff000000) >> 24);

    out_file->write(bitmap_header, size_of_bitmap_header + 14);
    delete[] bitmap_header;

    u8 fmt, r, g, b, a, r_int, g_int, b_int, a_int;
    u32 tex_id, curr_row, curr_col, max_row, max_col, arr_pos;
    in_tex.ordered_texel_data = new u32[in_tex.texel_count * 4];
    in_tex.tile_row_size = 4;
    in_tex.tile_column_size = 4;

    max_row = in_tex.height / 4;
    max_col = in_tex.width / 4;
    curr_row = 0;
    curr_col = 0;
    tex_id = 0;

    if (in_tex.texture_type == 0) //RGB5A3
    {
        for (u32 ind = 0; ind < in_tex.texel_count; ind++)
        {
            //Order and Write Texels
            /*
                4x4 Texel Shape:
                0 1 2 3
                4 5 6 7
                8 9 A B
                C D E F
            */
            tex_id = ind % 16;
            fmt = (u8)(in_tex.texel_data_rgb5a3[ind] >> 15) & 0x1;
            switch (fmt)
            {
            case 0: //Transparent Tex 4/4/4/3 RGBA
                a_int = (u8)((in_tex.texel_data_rgb5a3[ind] >> 12) & 0x7);
                a = (u8)(a_int << 5) | (a_int << 2) | (a_int >> 1);
                r_int = (u8)((in_tex.texel_data_rgb5a3[ind] >> 8) & 0xf);
                r = (u8)(r_int << 4) | (r_int);
                g_int = (u8)((in_tex.texel_data_rgb5a3[ind] >> 4) & 0xf);
                g = (u8)(g_int << 4) | (g_int);
                b_int = (u8)((in_tex.texel_data_rgb5a3[ind]) & 0xf);
                b = (u8)(b_int << 4) | (b_int);
                break;
            case 1: //Opaque Tex, RGB555
                r_int = (u8)(((in_tex.texel_data_rgb5a3[ind] >> 10)) & 0x1f);
                r = (u8)(r_int << 3 & 0xf8) | (r_int >> 2 & 0x07);
                g_int = (u8)(((in_tex.texel_data_rgb5a3[ind] >> 5)) & 0x1f);
                g = (u8)(g_int << 3 & 0xf8) | (g_int >> 2 & 0x07);
                b_int = (u8)((in_tex.texel_data_rgb5a3[ind]) & 0x1f);
                b = (u8)(b_int << 3 & 0xf8) | (b_int >> 2 & 0x07);
                a = (u8)0xff;
                break;
            default:
                std::cout << "Error in Texel Data" << std::endl;
                out_file->close();
                log_file->close();
                return 1;
            }
            /*r = 0;
            g = 0;
            b = 0;
            */
            //RGBA format
            arr_pos = (curr_row * in_tex.width) + (curr_col * in_tex.tile_row_size) + (tex_id % 4);
            in_tex.ordered_texel_data[arr_pos] = (u32)(((r & 0xff) << 24) | ((g & 0xff) << 16) | ((b & 0xff) << 8) | (a & 0xff));

            //log_file << "Row: " << curr_row << " - Col: " << curr_col << " - tex_id: " << tex_id << " - Arr Pos: " << arr_pos << "\n";

            //printf("Row: %u - Col: %u - tex_id: %u - Arr Pos: %u\n", curr_row, curr_col, tex_id, arr_pos);

            if (tex_id % 4 == 3 && tex_id != 15) { curr_row++; }
            if (tex_id == 15)
            {
                curr_col++;
                curr_row -= 3;
            }

            if (curr_col >= max_col)
            {
                curr_row += 4;
                curr_col = 0;
            }
        }
        delete[] in_tex.texel_data_rgb5a3;
    }
    else if (in_tex.texture_type == 0x02) //CMP
    {
        //First 32 bits of a chunk define the 2 RGB colours
        //Next 32 bits define the 1-bit alpha and the colour
        //RGB565 for colour0 and colour1 (in that order)
        //colour 2 and 3 are interpreted
        //Texels are 2 bits for up to 4 colours
        //32 byte "chunk" = 8x8 texels
        //= 4 blocks of 4x4 in order:
        /*
            0 1
            2 3
        */
        u16 r1, g1, b1, r2, g2, b2, r1_int, r2_int, g1_int, g2_int, b1_int, b2_int, a1, a2;
        u16 r3, g3, b3, r4, g4, b4, a3, a4;
        u8 col_data, shift;
        u16 col1, col2;
        u64 def_col1, def_col2, def_col3, def_col4;

        curr_row = 0;
        curr_col = 0;
        for (u32 curr_chunk = 0; curr_chunk < in_tex.chunk_count_cmp; curr_chunk++)
        {
            for (u32 curr_block = 0; curr_block < 4; curr_block++)
            {
                //First read the colours
                col1 = (in_tex.texel_data_cmp[(curr_chunk * in_tex.chunk_size_cmp) + (curr_block * (in_tex.chunk_size_cmp / 4))] << 8) |
                        (in_tex.texel_data_cmp[(curr_chunk * in_tex.chunk_size_cmp) + (curr_block * (in_tex.chunk_size_cmp / 4)) + 1]) & 0xff;
                col2 = (in_tex.texel_data_cmp[(curr_chunk * in_tex.chunk_size_cmp) + (curr_block * (in_tex.chunk_size_cmp / 4)) + 2] << 8) |
                        (in_tex.texel_data_cmp[(curr_chunk * in_tex.chunk_size_cmp) + (curr_block * (in_tex.chunk_size_cmp / 4)) + 3]) & 0xff;

                r1_int = (col1 >> 11) & 0x1f;
                r1 = (r1_int << 3) | (r1_int >> 2) & 0xff;
                g1_int = (col1 >> 5) & 0x3f;
                g1 = (g1_int << 2) | (g1_int >> 4) & 0xff;
                b1_int = (col1) & 0x1f;
                b1 = (b1_int << 3) | (b1_int >> 2) & 0xff;
                a1 = 0xff;
                def_col1 = (u32)(((r1 & 0xff) << 24) | ((g1 & 0xff) << 16) | ((b1 & 0xff) << 8) | (a1 & 0xff));

                r2_int = (col2 >> 11) & 0x1f;
                r2 = (r2_int << 3) | (r2_int >> 2) & 0xff;
                g2_int = (col2 >> 5) & 0x3f;
                g2 = (g2_int << 2) | (g2_int >> 4) & 0xff;
                b2_int = (col2) & 0x1f;
                b2 = (b2_int << 3) | (b2_int >> 2) & 0xff;
                a2 = 0xff;
                def_col2 = (u32)(((r2 & 0xff) << 24) | ((g2 & 0xff) << 16) | ((b2 & 0xff) << 8) | (a2 & 0xff));

                if (col1 > col2)
                {
                    //log_file << "Mode 1\n";
                    r3 = (u16)((2 * r1) + r2 + 1) / 3;
                    g3 = (u16)((2 * g1) + g2 + 1) / 3;
                    b3 = (u16)((2 * b1) + b2 + 1) / 3;
                    a3 = 0xff;
                    def_col3 = (u32)((r3 & 0xff) << 24) | ((g3 & 0xff) << 16) | ((b3 & 0xff) << 8) | (a3 & 0xff);

                    r4 = (u16)((2 * r2) + r1 + 1) / 3;
                    g4 = (u16)((2 * g2) + g1 + 1) / 3;
                    b4 = (u16)((2 * b2) + b1 + 1) / 3;
                    a4 = 0xff;
                    def_col4 = (u32)((r4 & 0xff) << 24) | ((g4 & 0xff) << 16) | ((b4 & 0xff) << 8) | (a4 & 0xff);
                }
                else
                {
                    //log_file << "Mode 2\n";
                    r3 = (u16)(r1 + r2) / 2;
                    g3 = (u16)(g1 + g2 + 1) / 2;
                    b3 = (u16)(b1 + b2 + 1) / 2;
                    a3 = 0xff;
                    def_col3 = (u32)((r3 & 0xff) << 24) | ((g3 & 0xff) << 16) | ((b3 & 0xff) << 8) | (a3 & 0xff);

                    r4 = (u16)((2 * r2) + r1 + 1) / 3;
                    g4 = (u16)((2 * g2) + g1 + 1) / 3;
                    b4 = (u16)((2 * b2) + b1 + 1) / 3;
                    a4 = 0x00;
                    def_col4 = (u32)((r4 & 0xff) << 24) | ((g4 & 0xff) << 16) | ((b4 & 0xff) << 8) | (a4 & 0xff);
                }

                /*log_file << "R1: " << (u32)(r1&0xff) << " - G1: " << (u32)(g1&0xff) << " - B1: " << (u32)(b1&0xff) << "\n"
                            << "R2: " << (u32)(r2&0xff) << " - G2: " << (u32)(g2&0xff) << " - B2: " << (u32)(b2&0xff) << "\n"
                            << "R3: " << (u32)(r3&0xff) << " - G3: " << (u32)(g3&0xff) << " - B3: " << (u32)(b3&0xff) << "\n"
                            << "R4: " << (u32)(r4&0xff) << " - G4: " << (u32)(g4&0xff) << " - B4: " << (u32)(b4&0xff) << "\n\n";*/

                for (u8 c_byte = 0; c_byte < 4; c_byte++)
                {
                    for (u8 c_tex = 0; c_tex < 4; c_tex++)
                    {
                        shift = (c_tex * 2);
                        col_data = (in_tex.texel_data_cmp[(curr_chunk * in_tex.chunk_size_cmp) + (curr_block * (in_tex.chunk_size_cmp / 4)) + 4 + c_byte] &
                                    (0xC0 >> shift)) >> (6 - shift);
                        col_data = ((col_data & 0x1) << 1) | ((col_data & 0x2) >> 1);

                        switch (col_data & 0x03)
                        {
                        case 0: //Colour 0, RGBA
                            in_tex.ordered_texel_data[(curr_row * in_tex.width) + (curr_col * in_tex.tile_row_size) + c_tex]
                                                        = (u32)def_col1;
                            break;
                        case 1: //Colour 1, RGBA
                            in_tex.ordered_texel_data[(curr_row * in_tex.width) + (curr_col * in_tex.tile_row_size) + c_tex]
                                                        = (u32)def_col2;
                            break;
                        case 2: //Colour 2, RGBA
                            in_tex.ordered_texel_data[(curr_row * in_tex.width) + (curr_col * in_tex.tile_row_size) + c_tex]
                                                        = (u32)def_col3;
                            break;
                        case 3: //Colour 3, RGBA
                            in_tex.ordered_texel_data[(curr_row * in_tex.width) + (curr_col * in_tex.tile_row_size) + c_tex]
                                                        = (u32)def_col4;
                            break;
                        default:
                            std::cout << "Error in Colour Data" << std::endl;
                            out_file->close();
                            log_file->close();
                            return 1;
                        }

                        //log_file << "Row: " << curr_row << " - Col: " << curr_col << " - c_byte: " << (u32)c_byte << " - c_tex: " << (u32)c_tex << " - Pos: " << (u32)(curr_row * in_tex.width) + (curr_col * in_tex.tile_row_size) + c_tex << " - col_data: " << (u32)col_data << "\n";
                    }
                    curr_row++;
                }
                curr_row -= 4;
                curr_col++;

                if (curr_block == 1)
                {
                    curr_col -= 2;
                    curr_row += 4;
                }
                else if (curr_block == 3)
                {
                    curr_row -= 4;
                }

                if (curr_col >= max_col)
                {
                    curr_col = 0;
                    curr_row += 8;
                }
            }
        }
        delete[] in_tex.texel_data_cmp;
    }

    for (u32 h = in_tex.height; h > 0; h--)
    {
        for (u32 w = 0; w < in_tex.width; w++)
        {
            //printf("h: %.4X - w: %.4X\n", h-1, w);
            out_file->write((char*)&(in_tex.ordered_texel_data[((h-1) * in_tex.width) + w]), 4);
        }
    }

    delete[] in_tex.ordered_texel_data;

    return 0;
}

int main(int argc, char* argv[])
{
    std::string in_file_path;
    std::string dir_path;
    bool dir = false;

    if (argc != 3)
    {
        std::cout << "Usage: " << argv[0] << " <[-f filename] | [-d directory_path]>\n";
        return 0;
    }
    else
    {
        std::string arg = argv[1];
        std::string path = argv[2];
        if (arg == "-f")
        {
            in_file_path = path;
            dir = false;
        }
        else if (arg == "-d")
        {
            dir_path = path;
            dir = true;
        }
    }

    std::fstream log_file;
    log_file.open("log.txt", std::fstream::out);

    if (dir)
    {
        std::string out_file_path;
        for (std::filesystem::directory_entry p: std::filesystem::directory_iterator(dir_path))
        {
            if (p.path().extension().string() != ".gtx")
            {
                log_file << "Skipping " << p.path().filename() << "\n";
                continue;
            }

            in_file_path = p.path().string();
            out_file_path = in_file_path.substr(0, in_file_path.length() - 4).append(".bmp");

            std::fstream in_file;
            in_file.open(in_file_path, std::fstream::in | std::fstream::binary);

            if (!in_file)
            {
                std::cout << "Error Loading File" << std::endl;
                return 1;
            }

            std::fstream out_file;
            out_file.open(out_file_path, std::fstream::out | std::fstream::binary);

            log_file << "File: " << in_file_path << "\n";

            u32 ret = processFile(&in_file, &out_file, &log_file);
            in_file.close();
            out_file.close();

            if (ret != 0)
            {
                log_file << "!!!Non-Zero Return Value for File: " << in_file_path << "\n";
            }
        }
        log_file.close();
        return 0;
    }
    else
    {
        std::string out_file_path = in_file_path.substr(0, in_file_path.length() - 4).append(".bmp");

        std::fstream in_file;
        in_file.open(in_file_path, std::fstream::in | std::fstream::binary);

        if (!in_file)
        {
            std::cout << "Error Loading File" << std::endl;
            return 1;
        }

        std::fstream out_file;
        out_file.open(out_file_path, std::fstream::out | std::fstream::binary);

        log_file << "File: " << in_file_path << "\n";

        u32 ret = processFile(&in_file, &out_file, &log_file);
        out_file.close();
        log_file.close();
        return ret;
    }

    return 0;
}
