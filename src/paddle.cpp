/*
    Copyright (C) 2008 Andrew Caudwell (acaudwell@gmail.com)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version
    3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "paddle.h"
#include "requestball.h"
#include "settings.h"

/* needed for png texture function */
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define PNG_SKIP_SETJMP_CHECK
#include <png.h>

#include "core/stringhash.h"

Paddle::Paddle(vec2 pos, vec4 colour, std::string token, FXFont font) {
    this->token = token;

// TODO: fix colouring
//    this->token_colour = token.size() > 0 ? colourHash2(token) : vec3(0.5,0.5,0.5);
    this->token_colour = token.size() > 0 ? colourHash(token) : vec3(0.5,0.5,0.5);

    this->pos = pos;
    this->lastcol = colour;
    this->default_colour = colour;
    this->colour  = lastcol;
    this->width = 10;
    this->height = 50;
    this->target = 0;

    font.alignTop(true);
    font.alignRight(true);
    font.dropShadow(true);

    this->font = font;

    dest_y = -1;
}

Paddle::~Paddle() {
}

void Paddle::moveTo(int y, float eta, vec4 nextcol) {
    this->start_y = (int) this->pos.y;
    this->dest_y = y;
    this->dest_eta = eta;
    this->dest_elapsed = 0.0f;
    this->nextcol = nextcol;

    //debugLog("move to %d over %.2f\n", dest_y, dest_eta);
}

bool Paddle::visible() {
    return colour.w > 0.01;
}

bool Paddle::moving() {
    return dest_y != -1;
}

float Paddle::getY() {
    return pos.y;
}

float Paddle::getX() {
    return pos.x;
}

RequestBall* Paddle::getTarget() {
    return target;
}

void Paddle::setTarget(RequestBall* target) {
    this->target = target;

    if(target==0) {
        moveTo(display.height/2, 4, default_colour);
        return;
    }

    vec2 dest = target->finish();
    vec4 col  = (settings.paddle_mode == PADDLE_VHOST || settings.paddle_mode == PADDLE_PID)  ?
        vec4(token_colour,1.0) : vec4(target->colour, 1.0f);

    moveTo((int)dest.y, target->arrivalTime(), col);
}

bool Paddle::mouseOver(TextArea& textarea, vec2& mouse) {

    if(pos.x <= mouse.x && pos.x + width >= mouse.x && abs(pos.y - mouse.y) < height/2) {

        std::vector<std::string> content;

        content.push_back( token );

        textarea.setText(content);
        textarea.setPos(mouse);
        textarea.setColour(vec3(colour));

        return true;
    }

    return false;
}

void Paddle::logic(float dt) {

    if(dest_y != -1) {
        float remaining = dest_eta - dest_elapsed;

        if(remaining<0.0f) {
            //debugLog("paddle end point reached\n");
            pos.y = dest_y;
            dest_y = -1;
            target = 0;
            colour = nextcol;
            lastcol = colour;
        } else {
            float alpha = remaining/dest_eta;
            pos.y = start_y + ((dest_y-start_y)*(1.0f - alpha));
            colour = lastcol * alpha + nextcol * (1.0f - alpha);
        }

        dest_elapsed += dt;
    }
}

void Paddle::drawToken() {
    font.setColour(colour);
    font.draw(pos.x-10, pos.y - (font.getMaxHeight()/2), token);
}

void Paddle::drawShadow() {

    vec2 spos = vec2(pos.x + 1.0f, pos.y + 1.0f);

    glColor4f(0.0, 0.0, 0.0, 0.7 * colour.w);
    glBegin(GL_QUADS);
        glVertex2f(spos.x,spos.y-(height/2));
        glVertex2f(spos.x,spos.y+(height/2));
        glVertex2f(spos.x+width,spos.y+(height/2));
        glVertex2f(spos.x+width,spos.y-(height/2));
    glEnd();
}

void Paddle::draw() {

    glColor4fv(glm::value_ptr(colour));
    glBegin(GL_QUADS);
        glVertex2f(pos.x,pos.y-(height/2));
        glVertex2f(pos.x,pos.y+(height/2));
        glVertex2f(pos.x+width,pos.y+(height/2));
        glVertex2f(pos.x+width,pos.y-(height/2));
    glEnd();
}

/* TODO: clean this up and move it to a new file */
GLuint png_texture_load(const char * file_name, int * width, int * height)
{
    png_byte header[8];

    FILE *fp = fopen(file_name, "rb");
    if (fp == 0)
    {
        perror(file_name);
        return 0;
    }

    // read the header
    fread(header, 1, 8, fp);

    if (png_sig_cmp(header, 0, 8))
    {
        fprintf(stderr, "error: %s is not a PNG.\n", file_name);
        fclose(fp);
        return 0;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fprintf(stderr, "error: png_create_read_struct returned 0.\n");
        fclose(fp);
        return 0;
    }

    // create png info struct
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        fprintf(stderr, "error: png_create_info_struct returned 0.\n");
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        fclose(fp);
        return 0;
    }

    // create png info struct
    png_infop end_info = png_create_info_struct(png_ptr);
    if (!end_info)
    {
        fprintf(stderr, "error: png_create_info_struct returned 0.\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
        fclose(fp);
        return 0;
    }

    // the code in this if statement gets called if libpng encounters an error
    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "error from libpng\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return 0;
    }

    // init png reading
    png_init_io(png_ptr, fp);

    // let libpng know you already read the first 8 bytes
    png_set_sig_bytes(png_ptr, 8);

    // read all the info up to the image data
    png_read_info(png_ptr, info_ptr);

    // variables to pass to get info
    int bit_depth, color_type;
    png_uint_32 temp_width, temp_height;

    // get info about png
    png_get_IHDR(png_ptr, info_ptr, &temp_width, &temp_height, &bit_depth, &color_type,
        NULL, NULL, NULL);

    if (width){ *width = temp_width; }
    if (height){ *height = temp_height; }

    //printf("%s: %lux%lu %d\n", file_name, temp_width, temp_height, color_type);

    if (bit_depth != 8)
    {
        fprintf(stderr, "%s: Unsupported bit depth %d.  Must be 8.\n", file_name, bit_depth);
        return 0;
    }

    GLint format;
    switch(color_type)
    {
    case PNG_COLOR_TYPE_RGB:
        format = GL_RGB;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
        format = GL_RGBA;
        break;
    default:
        fprintf(stderr, "%s: Unknown libpng color type %d.\n", file_name, color_type);
        return 0;
    }

    // Update the png info struct.
    png_read_update_info(png_ptr, info_ptr);

    // Row size in bytes.
    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    // glTexImage2d requires rows to be 4-byte aligned
    rowbytes += 3 - ((rowbytes-1) % 4);

    // Allocate the image_data as a big block, to be given to opengl
    png_byte * image_data = (png_byte *)malloc(rowbytes * temp_height * sizeof(png_byte)+15);
    if (image_data == NULL)
    {
        fprintf(stderr, "error: could not allocate memory for PNG image data\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return 0;
    }

    // row_pointers is for pointing to image_data for reading the png with libpng
    png_byte ** row_pointers = (png_byte **)malloc(temp_height * sizeof(png_byte *));
    if (row_pointers == NULL)
    {
        fprintf(stderr, "error: could not allocate memory for PNG row pointers\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        free(image_data);
        fclose(fp);
        return 0;
    }

    // set the individual row_pointers to point at the correct offsets of image_data
    for (unsigned int i = 0; i < temp_height; i++)
    {
        row_pointers[temp_height - 1 - i] = image_data + i * rowbytes;
    }

    // read the png into image_data through row_pointers
    png_read_image(png_ptr, row_pointers);

    // Generate the OpenGL texture object
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, temp_width, temp_height, 0, format, GL_UNSIGNED_BYTE, image_data);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // clean up
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    free(image_data);
    free(row_pointers);
    fclose(fp);
    return texture;
}

void Paddle::drawDoge() {
    static int heighto = 50;
    static int widtho = 40;
    GLuint texture = png_texture_load("dogeface.png", NULL, NULL);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f,1.0f); glVertex2f(pos.x,pos.y-(heighto/2));
        glTexCoord2f(0.0f,0.0f); glVertex2f(pos.x,pos.y+(heighto/2));
        glTexCoord2f(1.0f,0.0f); glVertex2f(pos.x+widtho,pos.y+(heighto/2));
        glTexCoord2f(1.0f,1.0f); glVertex2f(pos.x+widtho,pos.y-(heighto/2));
    glEnd();
}
