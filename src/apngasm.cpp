#include "apngasm.h"
#include <string>
using namespace std;


//Construct APNGAsm object
APNGAsm::APNGAsm(void){}

//Construct APNGAsm object
APNGAsm::APNGAsm(const vector<APNGFrame> &frames)
{
  this->frames.insert(this->frames.end(), frames.begin(), frames.end());
}

string APNGAsm::version(void)
{
  return APNGASM_VERSION;
}

size_t APNGAsm::frameCount()
{
  return frames.size();
}

size_t APNGAsm::reset()
{
  if (frames.empty())
    return 0;

  for (size_t n = 0; n < frames.size(); ++n)
  {
    if (frames[n].p != NULL)
      free(frames[n].p);
  }
  frames.clear();

  return frames.size();
}

//Adds a frame from a file
//Returns the frame number in the frame vector
//Uses default delay of 10ms if not specified
size_t APNGAsm::addFrame(const string &filePath, unsigned int delay_num, unsigned int delay_den)
{
	APNGFrame frame = new APNGFrame(filePath, delay_num, delay_den);
	frames.push_back(frame);
  return frames.size();
}

//Adds an APNGFrame object to the frame vector
//Returns the frame number in the frame vector
size_t APNGAsm::addFrame(const APNGFrame &frame)
{
  frames.push_back(frame);
  return frames.size();
}

//Adds an APNGFrame object to the frame vector
//Returns a frame vector with the added frames
APNGAsm& APNGAsm::operator << (const APNGFrame &frame)
{
  addFrame(frame);
  return *this;
}

//Loads an animation spec from JSON or XML
//Returns a frame vector with the loaded frames
//Loaded frames are added to the end of the frame vector
const vector<APNGFrame>& APNGAsm::loadAnimationSpec(const string &filePath)
{
  const bool isJSON = true;
  const vector<APNGFrame> &newFrames = isJSON ? loadJSONSpec(filePath) : loadXMLSpec(filePath);
  frames.insert(frames.end(), newFrames.begin(), newFrames.end());
  return frames;
}

unsigned char APNGAsm::FindCommonType(void)
{
  unsigned char coltype = frames[0].t;

  for (size_t n = 1; n < frames.size(); ++n)
  {
    if (frames[0].ps != frames[n].ps || memcmp(frames[0].pl, frames[n].pl, frames[0].ps*3) != 0)
      coltype = 6;
    else
    if (frames[0].ts != frames[n].ts || memcmp(frames[0].tr, frames[n].tr, frames[0].ts) != 0)
      coltype = 6;
    else
    if (frames[n].t != 3)
    {
      if (coltype != 3)
        coltype |= frames[n].t;
      else
        coltype = 6;
    }
    else
      if (coltype != 3)
        coltype = 6;
  }
  return coltype;
}

int APNGAsm::UpconvertToCommonType(unsigned char coltype)
{
  unsigned char  * sp;
  unsigned char  * dp;
  unsigned char    r, g, b;
  unsigned int     j;

  for (size_t n = 0; n < frames.size(); ++n)
  {
    if (coltype == 6 && frames[n].t != 6)
    {
      unsigned char * dst = (unsigned char *)malloc(m_size*4);
      if (dst == NULL)
        return 1;

      if (frames[n].t == 0)
      {
        sp = frames[n].p;
        dp = dst;
        if (frames[n].ts == 0)
        for (j=0; j<m_size; j++)
        {
          *dp++ = *sp;
          *dp++ = *sp;
          *dp++ = *sp++;
          *dp++ = 255;
        }
        else
        for (j=0; j<m_size; j++)
        {
          g = *sp++;
          *dp++ = g;
          *dp++ = g;
          *dp++ = g;
          *dp++ = (frames[n].tr[1] == g) ? 0 : 255;
        }
      }
      else
      if (frames[n].t == 2)
      {
        sp = frames[n].p;
        dp = dst;
        if (frames[n].ts == 0)
        for (j=0; j<m_size; j++)
        {
          *dp++ = *sp++;
          *dp++ = *sp++;
          *dp++ = *sp++;
          *dp++ = 255;
        }
        else
        for (j=0; j<m_size; j++)
        {
          r = *sp++;
          g = *sp++;
          b = *sp++;
          *dp++ = r;
          *dp++ = g;
          *dp++ = b;
          *dp++ = (frames[n].tr[1] == r && frames[n].tr[3] == g && frames[n].tr[5] == b) ? 0 : 255;
        }
      }
      else
      if (frames[n].t == 3)
      {
        sp = frames[n].p;
        dp = dst;
        for (j=0; j<m_size; j++, sp++)
        {
          *dp++ = frames[n].pl[*sp].r;
          *dp++ = frames[n].pl[*sp].g;
          *dp++ = frames[n].pl[*sp].b;
          *dp++ = frames[n].tr[*sp];
        }
      }
      else
      if (frames[n].t == 4)
      {
        sp = frames[n].p;
        dp = dst;
        for (j=0; j<m_size; j++)
        {
          *dp++ = *sp;
          *dp++ = *sp;
          *dp++ = *sp++;
          *dp++ = *sp++;
        }
      }
      free(frames[n].p);
      frames[n].p = dst;
    }
    else
    if (coltype == 4 && frames[n].t == 0)
    {
      unsigned char * dst = (unsigned char *)malloc(m_size*2);
      if (dst == NULL)
        return 1;

      sp = frames[n].p;
      dp = dst;
      for (j=0; j<m_size; j++)
      {
        *dp++ = *sp++;
        *dp++ = 255;
      }
      free(frames[n].p);
      frames[n].p = dst;
    }
    else
    if (coltype == 2 && frames[n].t == 0)
    {
      unsigned char * dst = (unsigned char *)malloc(m_size*3);
      if (dst == NULL)
        return 1;

      sp = frames[n].p;
      dp = dst;
      for (j=0; j<m_size; j++)
      {
        *dp++ = *sp;
        *dp++ = *sp;
        *dp++ = *sp++;
      }
      free(frames[n].p);
      frames[n].p = dst;
    }
  }
  return 0;
}

void APNGAsm::DirtyTransparencyOptimization(unsigned char coltype)
{
  if (coltype == 6)
  {
    for (size_t n = 0; n < frames.size(); ++n)
    {
      unsigned char * sp = frames[n].p;
      for (unsigned int j=0; j<m_size; j++, sp+=4)
        if (sp[3] == 0)
           sp[0] = sp[1] = sp[2] = 0;
    }
  }
  else
  if (coltype == 4)
  {
    for (size_t n = 0; n < frames.size(); ++n)
    {
      unsigned char * sp = frames[n].p;
      for (unsigned int j=0; j<m_size; j++, sp+=2)
        if (sp[1] == 0)
          sp[0] = 0;
    }
  }
}

unsigned char APNGAsm::DownconvertOptimizations(unsigned char coltype, bool keep_palette, bool keep_coltype)
{
  unsigned int     has_tcolor = 0;
  unsigned int     colors = 0;
  unsigned char  * sp;
  unsigned char  * dp;
  unsigned char    r, g, b, a;
  unsigned int     i, j, k;
  unsigned char    cube[4096];
  unsigned char    gray[256];
  COLORS           col[256];

  memset(&cube, 0, sizeof(cube));
  memset(&gray, 0, sizeof(gray));

  for (i=0; i<256; i++)
  {
    col[i].num = 0;
    col[i].r = col[i].g = col[i].b = i;
    col[i].a = m_trns[i] = 255;
  }
  m_palsize = 0;
  m_trnssize = 0;

  if (coltype == 6 && !keep_coltype)
  {
    int transparent = 255;
    int simple_trans = 1;
    int grayscale = 1;

    for (size_t n = 0; n < frames.size(); ++n)
    {
      sp = frames[n].p;
      for (j=0; j<m_size; j++)
      {
        r = *sp++;
        g = *sp++;
        b = *sp++;
        a = *sp++;
        transparent &= a;

        if (a != 0)
        {
          if (a != 255)
            simple_trans = 0;
          else
            if (((r | g | b) & 15) == 0)
              cube[(r<<4) + g + (b>>4)] = 1;

          if (r != g || g != b)
            grayscale = 0;
          else
            gray[r] = 1;
        }

        if (colors <= 256)
        {
          int found = 0;
          for (k=0; k<colors; k++)
          if (col[k].r == r && col[k].g == g && col[k].b == b && col[k].a == a)
          {
            found = 1;
            col[k].num++;
            break;
          }
          if (found == 0)
          {
            if (colors < 256)
            {
              col[colors].num++;
              col[colors].r = r;
              col[colors].g = g;
              col[colors].b = b;
              col[colors].a = a;
              if (a == 0) has_tcolor = 1;
            }
            colors++;
          }
        }
      }
    }

    if (grayscale && simple_trans && colors<=256) /* 6 -> 0 */
    {
      coltype = 0;

      for (i=0; i<256; i++)
      if (gray[i] == 0)
      {
        m_trns[0] = 0;
        m_trns[1] = i;
        m_trnssize = 2;
        break;
      }

      for (size_t n = 0; n < frames.size(); ++n)
      {
        sp = dp = frames[n].p;
        for (j=0; j<m_size; j++)
        {
          r = *sp++;
          g = *sp++;
          b = *sp++;
          if (*sp++ == 0)
            *dp++ = m_trns[1];
          else
            *dp++ = g;
        }
      }
    }
    else
    if (colors<=256)   /* 6 -> 3 */
    {
      coltype = 3;

      if (has_tcolor==0 && colors<256)
        col[colors++].a = 0;

      qsort(&col[0], colors, sizeof(COLORS), cmp_colors);

      m_palsize = colors;
      for (i=0; i<colors; i++)
      {
        m_palette[i].r = col[i].r;
        m_palette[i].g = col[i].g;
        m_palette[i].b = col[i].b;
        m_trns[i]      = col[i].a;
        if (m_trns[i] != 255) m_trnssize = i+1;
      }

      for (size_t n = 0; n < frames.size(); ++n)
      {
        sp = dp = frames[n].p;
        for (j=0; j<m_size; j++)
        {
          r = *sp++;
          g = *sp++;
          b = *sp++;
          a = *sp++;
          for (k=0; k<colors; k++)
            if (col[k].r == r && col[k].g == g && col[k].b == b && col[k].a == a)
              break;
          *dp++ = k;
        }
      }
    }
    else
    if (grayscale)     /* 6 -> 4 */
    {
      coltype = 4;
      for (size_t n = 0; n < frames.size(); ++n)
      {
        sp = dp = frames[n].p;
        for (j=0; j<m_size; j++)
        {
          r = *sp++;
          g = *sp++;
          *dp++ = *sp++;
          *dp++ = *sp++;
        }
      }
    }
    else
    if (simple_trans)  /* 6 -> 2 */
    {
      for (i=0; i<4096; i++)
      if (cube[i] == 0)
      {
        m_trns[0] = 0;
        m_trns[1] = (i>>4)&0xF0;
        m_trns[2] = 0;
        m_trns[3] = i&0xF0;
        m_trns[4] = 0;
        m_trns[5] = (i<<4)&0xF0;
        m_trnssize = 6;
        break;
      }
      if (transparent == 255)
      {
        coltype = 2;
        for (size_t n = 0; n < frames.size(); ++n)
        {
          sp = dp = frames[n].p;
          for (j=0; j<m_size; j++)
          {
            *dp++ = *sp++;
            *dp++ = *sp++;
            *dp++ = *sp++;
            sp++;
          }
        }
      }
      else
      if (m_trnssize != 0)
      {
        coltype = 2;
        for (size_t n = 0; n < frames.size(); ++n)
        {
          sp = dp = frames[n].p;
          for (j=0; j<m_size; j++)
          {
            r = *sp++;
            g = *sp++;
            b = *sp++;
            a = *sp++;
            if (a == 0)
            {
              *dp++ = m_trns[1];
              *dp++ = m_trns[3];
              *dp++ = m_trns[5];
            }
            else
            {
              *dp++ = r;
              *dp++ = g;
              *dp++ = b;
            }
          }
        }
      }
    }
  }
  else
  if (coltype == 2)
  {
    int grayscale = 1;

    for (size_t n = 0; n < frames.size(); ++n)
    {
      sp = frames[n].p;
      for (j=0; j<m_size; j++)
      {
        r = *sp++;
        g = *sp++;
        b = *sp++;

        if (frames[n].ts == 0)
          if (((r | g | b) & 15) == 0)
            cube[(r<<4) + g + (b>>4)] = 1;

        if (r != g || g != b)
          grayscale = 0;
        else
          gray[r] = 1;

        if (colors <= 256)
        {
          int found = 0;
          for (k=0; k<colors; k++)
          if (col[k].r == r && col[k].g == g && col[k].b == b)
          {
            found = 1;
            col[k].num++;
            break;
          }
          if (found == 0)
          {
            if (colors < 256)
            {
              col[colors].num++;
              col[colors].r = r;
              col[colors].g = g;
              col[colors].b = b;
              if (frames[n].ts == 6 && frames[n].tr[1] == r && frames[n].tr[3] == g && frames[n].tr[5] == b)
              {
                col[colors].a = 0;
                has_tcolor = 1;
              }
            }
            colors++;
          }
        }
      }
    }

    if (grayscale && colors<=256 && !keep_coltype) /* 2 -> 0 */
    {
      for (i=0; i<256; i++)
      if (gray[i] == 0)
      {
        m_trns[0] = 0;
        m_trns[1] = i;
        m_trnssize = 2;
        break;
      }
      if (frames[0].ts == 0)
      {
        coltype = 0;
        for (size_t n = 0; n < frames.size(); ++n)
        {
          sp = dp = frames[n].p;
          for (j=0; j<m_size; j++, sp+=3)
            *dp++ = *sp;
        }
      }
      else
      if (m_trnssize != 0)
      {
        coltype = 0;
        for (size_t n = 0; n < frames.size(); ++n)
        {
          sp = dp = frames[n].p;
          for (j=0; j<m_size; j++)
          {
            r = *sp++;
            g = *sp++;
            b = *sp++;
            if (frames[n].tr[1] == r && frames[n].tr[3] == g && frames[n].tr[5] == b)
              *dp++ = m_trns[1];
            else
              *dp++ = g;
          }
        }
      }
    }
    else
    if (colors<=256 && !keep_coltype)   /* 2 -> 3 */
    {
      coltype = 3;

      if (has_tcolor==0 && colors<256)
        col[colors++].a = 0;

      qsort(&col[0], colors, sizeof(COLORS), cmp_colors);

      m_palsize = colors;
      for (i=0; i<colors; i++)
      {
        m_palette[i].r = col[i].r;
        m_palette[i].g = col[i].g;
        m_palette[i].b = col[i].b;
        m_trns[i]      = col[i].a;
        if (m_trns[i] != 255) m_trnssize = i+1;
      }

      for (size_t n = 0; n < frames.size(); ++n)
      {
        sp = dp = frames[n].p;
        for (j=0; j<m_size; j++)
        {
          r = *sp++;
          g = *sp++;
          b = *sp++;

          for (k=0; k<colors; k++)
            if (col[k].r == r && col[k].g == g && col[k].b == b)
              break;
          *dp++ = k;
        }
      }
    }
    else /* 2 -> 2 */
    {
      if (frames[0].ts != 0)
      {
        memcpy(m_trns, frames[0].tr, frames[0].ts);
        m_trnssize = frames[0].ts;
      }
      else
      for (i=0; i<4096; i++)
      if (cube[i] == 0)
      {
        m_trns[0] = 0;
        m_trns[1] = (i>>4)&0xF0;
        m_trns[2] = 0;
        m_trns[3] = i&0xF0;
        m_trns[4] = 0;
        m_trns[5] = (i<<4)&0xF0;
        m_trnssize = 6;
        break;
      }
    }
  }
  else
  if (coltype == 4 && !keep_coltype)
  {
    int simple_trans = 1;

    for (size_t n = 0; n < frames.size(); ++n)
    {
      sp = frames[n].p;
      for (j=0; j<m_size; j++)
      {
        g = *sp++;
        a = *sp++;

        if (a != 0)
        {
          if (a != 255)
            simple_trans = 0;
          else
            gray[g] = 1;
        }

        if (colors <= 256)
        {
          int found = 0;
          for (k=0; k<colors; k++)
          if (col[k].g == g && col[k].a == a)
          {
            found = 1;
            col[k].num++;
            break;
          }
          if (found == 0)
          {
            if (colors < 256)
            {
              col[colors].num++;
              col[colors].r = g;
              col[colors].g = g;
              col[colors].b = g;
              col[colors].a = a;
              if (a == 0) has_tcolor = 1;
            }
            colors++;
          }
        }
      }
    }

    if (simple_trans && colors<=256)   /* 4 -> 0 */
    {
      coltype = 0;

      for (i=0; i<256; i++)
      if (gray[i] == 0)
      {
        m_trns[0] = 0;
        m_trns[1] = i;
        m_trnssize = 2;
        break;
      }

      for (size_t n = 0; n < frames.size(); ++n)
      {
        sp = dp = frames[n].p;
        for (j=0; j<m_size; j++)
        {
          g = *sp++;
          if (*sp++ == 0)
            *dp++ = m_trns[1];
          else
            *dp++ = g;
        }
      }
    }
    else
    if (colors<=256)   /* 4 -> 3 */
    {
      coltype = 3;

      if (has_tcolor==0 && colors<256)
        col[colors++].a = 0;

      qsort(&col[0], colors, sizeof(COLORS), cmp_colors);

      m_palsize = colors;
      for (i=0; i<colors; i++)
      {
        m_palette[i].r = col[i].r;
        m_palette[i].g = col[i].g;
        m_palette[i].b = col[i].b;
        m_trns[i]      = col[i].a;
        if (m_trns[i] != 255) m_trnssize = i+1;
      }

      for (size_t n = 0; n < frames.size(); ++n)
      {
        sp = dp = frames[n].p;
        for (j=0; j<m_size; j++)
        {
          g = *sp++;
          a = *sp++;
          for (k=0; k<colors; k++)
            if (col[k].g == g && col[k].a == a)
              break;
          *dp++ = k;
        }
      }
    }
  }
  else
  if (coltype == 3)
  {
    int simple_trans = 1;
    int grayscale = 1;

    for (int c=0; c<frames[0].ps; c++)
    {
      col[c].r = frames[0].pl[c].r;
      col[c].g = frames[0].pl[c].g;
      col[c].b = frames[0].pl[c].b;
      col[c].a = frames[0].tr[c];
    }

    for (size_t n = 0; n < frames.size(); ++n)
    {
      sp = frames[n].p;
      for (j=0; j<m_size; j++)
        col[*sp++].num++;
    }

    for (i=0; i<256; i++)
    if (col[i].num != 0)
    {
      colors = i+1;
      if (col[i].a != 0)
      {
        if (col[i].a != 255)
          simple_trans = 0;
        else
        if (col[i].r != col[i].g || col[i].g != col[i].b)
          grayscale = 0;
        else
          gray[col[i].g] = 1;
      }
      else
        has_tcolor = 1;
    }

    if (grayscale && simple_trans && !keep_coltype) /* 3 -> 0 */
    {
      for (i=0; i<256; i++)
      if (gray[i] == 0)
      {
        m_trns[0] = 0;
        m_trns[1] = i;
        m_trnssize = 2;
        break;
      }
      if (!has_tcolor)
      {
        coltype = 0;
        for (size_t n = 0; n < frames.size(); ++n)
        {
          sp = frames[n].p;
          for (j=0; j<m_size; j++, sp++)
            *sp = frames[n].pl[*sp].g;
        }
      }
      else
      if (m_trnssize != 0)
      {
        coltype = 0;
        for (size_t n = 0; n < frames.size(); ++n)
        {
          sp = frames[n].p;
          for (j=0; j<m_size; j++, sp++)
          {
            if (col[*sp].a == 0)
              *sp = m_trns[1];
            else
              *sp = frames[n].pl[*sp].g;
          }
        }
      }
    }
    else
    if (!keep_palette)                 /* 3 -> 3 */
    {
      for (i=0; i<256; i++)
      if (col[i].num == 0)
      {
        col[i].a = 255;
        if (!has_tcolor)
        {
          col[i].a = 0;
          has_tcolor = 1;
        }
      }

      qsort(&col[0], 256, sizeof(COLORS), cmp_colors);

      for (i=0; i<256; i++)
      {
        m_palette[i].r = col[i].r;
        m_palette[i].g = col[i].g;
        m_palette[i].b = col[i].b;
        m_trns[i]      = col[i].a;
        if (col[i].num != 0)
          m_palsize = i+1;
        if (m_trns[i] != 255)
          m_trnssize = i+1;
      }

      for (size_t n = 0; n < frames.size(); ++n)
      {
        sp = frames[n].p;
        for (j=0; j<m_size; j++)
        {
          r = frames[n].pl[*sp].r;
          g = frames[n].pl[*sp].g;
          b = frames[n].pl[*sp].b;
          a = frames[n].tr[*sp];

          for (k=0; k<m_palsize; k++)
            if (col[k].r == r && col[k].g == g && col[k].b == b && col[k].a == a)
              break;
          *sp++ = k;
        }
      }
    }
    else
    {
      m_palsize = frames[0].ps;
      m_trnssize = frames[0].ts;
      for (i=0; i<m_palsize; i++)
      {
        m_palette[i].r = col[i].r;
        m_palette[i].g = col[i].g;
        m_palette[i].b = col[i].b;
      }
      for (i=0; i<m_trnssize; i++)
        m_trns[i] = col[i].a;
    }
  }
  else
  if (coltype == 0)  /* 0 -> 0 */
  {
    if (frames[0].ts != 0)
    {
      memcpy(m_trns, frames[0].tr, frames[0].ts);
      m_trnssize = frames[0].ts;
    }
    else
    {
      for (size_t n = 0; n < frames.size(); ++n)
      {
        sp = frames[n].p;
        for (j=0; j<m_size; j++)
          gray[*sp++] = 1;
      }
      for (i=0; i<256; i++)
      if (gray[i] == 0)
      {
        m_trns[0] = 0;
        m_trns[1] = i;
        m_trnssize = 2;
        break;
      }
    }
  }
  return coltype;
}

bool APNGAsm::Save(const string &outputPath, unsigned char coltype, unsigned int first, unsigned int loops)
{
  unsigned int    j, k;
  unsigned int    has_tcolor = 0;
  unsigned int    tcolor = 0;
  unsigned int    i, rowbytes, imagesize;
  unsigned int    idat_size, zbuf_size, zsize;
  unsigned char * zbuf;
  FILE*           f;
  unsigned char   png_sign[8] = {137,  80,  78,  71,  13,  10,  26,  10};
  unsigned char   png_Software[27] = { 83, 111, 102, 116, 119, 97, 114, 101, '\0',
                                       65,  80,  78,  71,  32, 65, 115, 115, 101,
                                      109,  98, 108, 101, 114, 32,  50,  46,  56};

  unsigned int bpp = 1;
  if (coltype == 2)
    bpp = 3;
  else
  if (coltype == 4)
    bpp = 2;
  else
  if (coltype == 6)
    bpp = 4;

  if (coltype == 0)
  {
    if (m_trnssize)
    {
      has_tcolor = 1;
      tcolor = m_trns[1];
    }
  }
  else
  if (coltype == 2)
  {
    if (m_trnssize)
    {
      has_tcolor = 1;
      tcolor = (((m_trns[5]<<8)+m_trns[3])<<8)+m_trns[1];
    }
  }
  else
  if (coltype == 3)
  {
    for (i=0; i<m_trnssize; i++)
    if (m_trns[i] == 0)
    {
      has_tcolor = 1;
      tcolor = i;
      break;
    }
  }
  else
  {
    has_tcolor = 1;
    tcolor = 0;
  }

  rowbytes  = m_width * bpp;
  imagesize = rowbytes * m_height;

  unsigned char * temp  = (unsigned char *)malloc(imagesize);
  unsigned char * over1 = (unsigned char *)malloc(imagesize);
  unsigned char * over2 = (unsigned char *)malloc(imagesize);
  unsigned char * over3 = (unsigned char *)malloc(imagesize);
  unsigned char * rows  = (unsigned char *)malloc((rowbytes + 1) * m_height);

  if (!temp || !over1 || !over2 || !over3 || !rows)
    return false;

  if ((f = fopen(outputPath.c_str(), "wb")) != 0)
  {
    struct IHDR
    {
      unsigned int    mWidth;
      unsigned int    mHeight;
      unsigned char   mDepth;
      unsigned char   mColorType;
      unsigned char   mCompression;
      unsigned char   mFilterMethod;
      unsigned char   mInterlaceMethod;
    } ihdr = { swap32(m_width), swap32(m_height), 8, coltype, 0, 0, 0 };

    struct acTL
    {
      unsigned int    mFrameCount;
      unsigned int    mLoopCount;
    } actl = { swap32(frames.size()-first), swap32(loops) };

    struct fcTL
    {
      unsigned int    mSeq;
      unsigned int    mWidth;
      unsigned int    mHeight;
      unsigned int    mXOffset;
      unsigned int    mYOffset;
      unsigned short  mDelayNum;
      unsigned short  mDelayDen;
      unsigned char   mDisposeOp;
      unsigned char   mBlendOp;
    } fctl;

    fwrite(png_sign, 1, 8, f);

    write_chunk(f, "IHDR", (unsigned char *)(&ihdr), 13);

    if (frames.size() > 1)
      write_chunk(f, "acTL", (unsigned char *)(&actl), 8);
    else
      first = 0;

    if (m_palsize > 0)
      write_chunk(f, "PLTE", (unsigned char *)(&m_palette), m_palsize*3);

    if (m_trnssize > 0)
      write_chunk(f, "tRNS", m_trns, m_trnssize);

    op_zstream1.data_type = Z_BINARY;
    op_zstream1.zalloc = Z_NULL;
    op_zstream1.zfree = Z_NULL;
    op_zstream1.opaque = Z_NULL;
    deflateInit2(&op_zstream1, Z_BEST_SPEED+1, 8, 15, 8, Z_DEFAULT_STRATEGY);

    op_zstream2.data_type = Z_BINARY;
    op_zstream2.zalloc = Z_NULL;
    op_zstream2.zfree = Z_NULL;
    op_zstream2.opaque = Z_NULL;
    deflateInit2(&op_zstream2, Z_BEST_SPEED+1, 8, 15, 8, Z_FILTERED);

    fin_zstream.data_type = Z_BINARY;
    fin_zstream.zalloc = Z_NULL;
    fin_zstream.zfree = Z_NULL;
    fin_zstream.opaque = Z_NULL;
    deflateInit2(&fin_zstream, Z_BEST_COMPRESSION, 8, 15, 8, Z_DEFAULT_STRATEGY);

    idat_size = (rowbytes + 1) * m_height;
    zbuf_size = idat_size + ((idat_size + 7) >> 3) + ((idat_size + 63) >> 6) + 11;

    zbuf = (unsigned char *)malloc(zbuf_size);
    op_zbuf1 = (unsigned char *)malloc(zbuf_size);
    op_zbuf2 = (unsigned char *)malloc(zbuf_size);
    row_buf = (unsigned char *)malloc(rowbytes + 1);
    sub_row = (unsigned char *)malloc(rowbytes + 1);
    up_row = (unsigned char *)malloc(rowbytes + 1);
    avg_row = (unsigned char *)malloc(rowbytes + 1);
    paeth_row = (unsigned char *)malloc(rowbytes + 1);

    if (zbuf && op_zbuf1 && op_zbuf2 && row_buf && sub_row && up_row && avg_row && paeth_row)
    {
      row_buf[0] = 0;
      sub_row[0] = 1;
      up_row[0] = 2;
      avg_row[0] = 3;
      paeth_row[0] = 4;
    }
    else
      return false;

    unsigned int x0 = 0;
    unsigned int y0 = 0;
    unsigned int w0 = m_width;
    unsigned int h0 = m_height;
    unsigned char bop = 0;
    unsigned char dop = 0;
    m_next_seq_num = 0;

    for (j=0; j<6; j++)
      op[j].valid = 0;
    deflate_rect_op(frames[0].p, x0, y0, w0, h0, bpp, rowbytes, zbuf_size, 0);
    deflate_rect_fin(zbuf, &zsize, bpp, rowbytes, rows, zbuf_size, 0);

    if (first)
    {
      write_IDATs(f, 0, zbuf, zsize, idat_size);

      for (j=0; j<6; j++)
        op[j].valid = 0;
      deflate_rect_op(frames[1].p, x0, y0, w0, h0, bpp, rowbytes, zbuf_size, 0);
      deflate_rect_fin(zbuf, &zsize, bpp, rowbytes, rows, zbuf_size, 0);
    }

    for (size_t n = first; n < frames.size()-1; ++n)
    {
      unsigned int op_min;
      int          op_best;

      for (j=0; j<6; j++)
        op[j].valid = 0;

      /* dispose = none */
      get_rect(m_width, m_height, frames[n].p, frames[n+1].p, over1, bpp, rowbytes, zbuf_size, has_tcolor, tcolor, 0);

      /* dispose = background */
      if (has_tcolor)
      {
        memcpy(temp, frames[n].p, imagesize);
        if (coltype == 2)
          for (j=0; j<h0; j++)
            for (k=0; k<w0; k++)
              memcpy(temp + ((j+y0)*m_width + (k+x0))*3, &tcolor, 3);
        else
          for (j=0; j<h0; j++)
            memset(temp + ((j+y0)*m_width + x0)*bpp, tcolor, w0*bpp);

        get_rect(m_width, m_height, temp, frames[n+1].p, over2, bpp, rowbytes, zbuf_size, has_tcolor, tcolor, 1);
      }

      /* dispose = previous */
      if (n > first)
        get_rect(m_width, m_height, frames[n-1].p, frames[n+1].p, over3, bpp, rowbytes, zbuf_size, has_tcolor, tcolor, 2);

      op_min = op[0].size;
      op_best = 0;
      for (j=1; j<6; j++)
      {
        if (op[j].size < op_min && op[j].valid)
        {
          op_min = op[j].size;
          op_best = j;
        }
      }

      dop = op_best >> 1;

      fctl.mSeq       = swap32(m_next_seq_num++);
      fctl.mWidth     = swap32(w0);
      fctl.mHeight    = swap32(h0);
      fctl.mXOffset   = swap32(x0);
      fctl.mYOffset   = swap32(y0);
      fctl.mDelayNum  = swap16(frames[n].delay_num);
      fctl.mDelayDen  = swap16(frames[n].delay_den);
      fctl.mDisposeOp = dop;
      fctl.mBlendOp   = bop;
      write_chunk(f, "fcTL", (unsigned char *)(&fctl), 26);

      write_IDATs(f, n, zbuf, zsize, idat_size);

      /* process apng dispose - begin */
      if (dop == 1)
      {
        if (coltype == 2)
          for (j=0; j<h0; j++)
            for (k=0; k<w0; k++)
              memcpy(frames[n].p + ((j+y0)*m_width + (k+x0))*3, &tcolor, 3);
        else
          for (j=0; j<h0; j++)
            memset(frames[n].p + ((j+y0)*m_width + x0)*bpp, tcolor, w0*bpp);
      }
      else
      if (dop == 2)
      {
        for (j=0; j<h0; j++)
          memcpy(frames[n].p + ((j+y0)*m_width + x0)*bpp, frames[n-1].p + ((j+y0)*m_width + x0)*bpp, w0*bpp);
      }
      /* process apng dispose - end */

      x0 = op[op_best].x;
      y0 = op[op_best].y;
      w0 = op[op_best].w;
      h0 = op[op_best].h;
      bop = op_best & 1;

      deflate_rect_fin(zbuf, &zsize, bpp, rowbytes, rows, zbuf_size, op_best);
    }

    if (frames.size() > 1)
    {
      fctl.mSeq       = swap32(m_next_seq_num++);
      fctl.mWidth     = swap32(w0);
      fctl.mHeight    = swap32(h0);
      fctl.mXOffset   = swap32(x0);
      fctl.mYOffset   = swap32(y0);
      fctl.mDelayNum  = swap16(frames[frames.size()-1].delay_num);
      fctl.mDelayDen  = swap16(frames[frames.size()-1].delay_den);
      fctl.mDisposeOp = 0;
      fctl.mBlendOp   = bop;
      write_chunk(f, "fcTL", (unsigned char *)(&fctl), 26);
    }

    write_IDATs(f, frames.size()-1, zbuf, zsize, idat_size);

    write_chunk(f, "tEXt", png_Software, 27);
    write_chunk(f, "IEND", 0, 0);
    fclose(f);

    free(zbuf);
    free(op_zbuf1);
    free(op_zbuf2);
    free(row_buf);
    free(sub_row);
    free(up_row);
    free(avg_row);
    free(paeth_row);
  }
  else
    return false;

  free(temp);
  free(over1);
  free(over2);
  free(over3);
  free(rows);

  return true;
}

//Assembles and outputs an APNG file
//Returns the assembled file object
//If no output path is specified only the file object is returned
bool APNGAsm::assemble(const string &outputPath)
{
  if (frames.empty())
    return false;

  m_width  = frames[0].w;
  m_height = frames[0].h;
  m_size   = m_width * m_height;

  unsigned char coltype = FindCommonType();

  if (UpconvertToCommonType(coltype))
    return false;

  DirtyTransparencyOptimization(coltype);

  coltype = DownconvertOptimizations(coltype, false, false);

  return Save(outputPath, coltype, 0, 0);
}

void APNGAsm::process_rect(unsigned char * row, int rowbytes, int bpp, int stride, int h, unsigned char * rows)
{
  int i, j, v;
  int a, b, c, pa, pb, pc, p;
  unsigned char * prev = NULL;
  unsigned char * dp  = rows;
  unsigned char * out;

  for (j=0; j<h; j++)
  {
    unsigned int    sum = 0;
    unsigned char * best_row = row_buf;
    unsigned int    mins = ((unsigned int)(-1)) >> 1;

    out = row_buf+1;
    for (i=0; i<rowbytes; i++)
    {
      v = out[i] = row[i];
      sum += (v < 128) ? v : 256 - v;
    }
    mins = sum;

    sum = 0;
    out = sub_row+1;
    for (i=0; i<bpp; i++)
    {
      v = out[i] = row[i];
      sum += (v < 128) ? v : 256 - v;
    }
    for (i=bpp; i<rowbytes; i++)
    {
      v = out[i] = row[i] - row[i-bpp];
      sum += (v < 128) ? v : 256 - v;
      if (sum > mins) break;
    }
    if (sum < mins)
    {
      mins = sum;
      best_row = sub_row;
    }

    if (prev)
    {
      sum = 0;
      out = up_row+1;
      for (i=0; i<rowbytes; i++)
      {
        v = out[i] = row[i] - prev[i];
        sum += (v < 128) ? v : 256 - v;
        if (sum > mins) break;
      }
      if (sum < mins)
      {
        mins = sum;
        best_row = up_row;
      }

      sum = 0;
      out = avg_row+1;
      for (i=0; i<bpp; i++)
      {
        v = out[i] = row[i] - prev[i]/2;
        sum += (v < 128) ? v : 256 - v;
      }
      for (i=bpp; i<rowbytes; i++)
      {
        v = out[i] = row[i] - (prev[i] + row[i-bpp])/2;
        sum += (v < 128) ? v : 256 - v;
        if (sum > mins) break;
      }
      if (sum < mins)
      {
        mins = sum;
        best_row = avg_row;
      }

      sum = 0;
      out = paeth_row+1;
      for (i=0; i<bpp; i++)
      {
        v = out[i] = row[i] - prev[i];
        sum += (v < 128) ? v : 256 - v;
      }
      for (i=bpp; i<rowbytes; i++)
      {
        a = row[i-bpp];
        b = prev[i];
        c = prev[i-bpp];
        p = b - c;
        pc = a - c;
        pa = abs(p);
        pb = abs(pc);
        pc = abs(p + pc);
        p = (pa <= pb && pa <=pc) ? a : (pb <= pc) ? b : c;
        v = out[i] = row[i] - p;
        sum += (v < 128) ? v : 256 - v;
        if (sum > mins) break;
      }
      if (sum < mins)
      {
        best_row = paeth_row;
      }
    }

    if (rows == NULL)
    {
      // deflate_rect_op()
      op_zstream1.next_in = row_buf;
      op_zstream1.avail_in = rowbytes + 1;
      deflate(&op_zstream1, Z_NO_FLUSH);

      op_zstream2.next_in = best_row;
      op_zstream2.avail_in = rowbytes + 1;
      deflate(&op_zstream2, Z_NO_FLUSH);
    }
    else
    {
      // deflate_rect_fin()
      memcpy(dp, best_row, rowbytes+1);
      dp += rowbytes+1;
    }

    prev = row;
    row += stride;
  }
}

void APNGAsm::deflate_rect_fin(unsigned char * zbuf, unsigned int * zsize, int bpp, int stride, unsigned char * rows, int zbuf_size, int n)
{
  unsigned char * row  = op[n].p + op[n].y*stride + op[n].x*bpp;
  int rowbytes = op[n].w*bpp;

  if (op[n].filters == 0)
  {
    deflateInit2(&fin_zstream, Z_BEST_COMPRESSION, 8, 15, 8, Z_DEFAULT_STRATEGY);
    unsigned char * dp  = rows;
    for (int j=0; j<op[n].h; j++)
    {
      *dp++ = 0;
      memcpy(dp, row, rowbytes);
      dp += rowbytes;
      row += stride;
    }
  }
  else
  {
    deflateInit2(&fin_zstream, Z_BEST_COMPRESSION, 8, 15, 8, Z_FILTERED);
    process_rect(row, rowbytes, bpp, stride, op[n].h, rows);
  }

  fin_zstream.next_out = zbuf;
  fin_zstream.avail_out = zbuf_size;
  fin_zstream.next_in = rows;
  fin_zstream.avail_in = op[n].h*(rowbytes + 1);
  deflate(&fin_zstream, Z_FINISH);
  *zsize = fin_zstream.total_out;
  deflateReset(&fin_zstream);
}

void APNGAsm::deflate_rect_op(unsigned char *pdata, int x, int y, int w, int h, int bpp, int stride, int zbuf_size, int n)
{
  unsigned char * row  = pdata + y*stride + x*bpp;
  int rowbytes = w * bpp;

  op_zstream1.data_type = Z_BINARY;
  op_zstream1.next_out = op_zbuf1;
  op_zstream1.avail_out = zbuf_size;

  op_zstream2.data_type = Z_BINARY;
  op_zstream2.next_out = op_zbuf2;
  op_zstream2.avail_out = zbuf_size;

  process_rect(row, rowbytes, bpp, stride, h, NULL);

  deflate(&op_zstream1, Z_FINISH);
  deflate(&op_zstream2, Z_FINISH);
  op[n].p = pdata;
  if (op_zstream1.total_out < op_zstream2.total_out)
  {
    op[n].size = op_zstream1.total_out;
    op[n].filters = 0;
  }
  else
  {
    op[n].size = op_zstream2.total_out;
    op[n].filters = 1;
  }
  op[n].x = x;
  op[n].y = y;
  op[n].w = w;
  op[n].h = h;
  op[n].valid = 1;
  deflateReset(&op_zstream1);
  deflateReset(&op_zstream2);
}

void APNGAsm::get_rect(unsigned int w, unsigned int h, unsigned char *pimage1, unsigned char *pimage2, unsigned char *ptemp, unsigned int bpp, unsigned int stride, int zbuf_size, unsigned int has_tcolor, unsigned int tcolor, int n)
{
  unsigned int   i, j, x0, y0, w0, h0;
  unsigned int   x_min = w-1;
  unsigned int   y_min = h-1;
  unsigned int   x_max = 0;
  unsigned int   y_max = 0;
  unsigned int   diffnum = 0;
  unsigned int   over_is_possible = 1;

  if (!has_tcolor)
    over_is_possible = 0;

  if (bpp == 1)
  {
    unsigned char *pa = pimage1;
    unsigned char *pb = pimage2;
    unsigned char *pc = ptemp;

    for (j=0; j<h; j++)
    for (i=0; i<w; i++)
    {
      unsigned char c = *pb++;
      if (*pa++ != c)
      {
        diffnum++;
        if (has_tcolor && c == tcolor) over_is_possible = 0;
        if (i<x_min) x_min = i;
        if (i>x_max) x_max = i;
        if (j<y_min) y_min = j;
        if (j>y_max) y_max = j;
      }
      else
        c = tcolor;

      *pc++ = c;
    }
  }
  else
  if (bpp == 2)
  {
    unsigned short *pa = (unsigned short *)pimage1;
    unsigned short *pb = (unsigned short *)pimage2;
    unsigned short *pc = (unsigned short *)ptemp;

    for (j=0; j<h; j++)
    for (i=0; i<w; i++)
    {
      unsigned int c1 = *pa++;
      unsigned int c2 = *pb++;
      if ((c1 != c2) && ((c1>>8) || (c2>>8)))
      {
        diffnum++;
        if ((c2 >> 8) != 0xFF) over_is_possible = 0;
        if (i<x_min) x_min = i;
        if (i>x_max) x_max = i;
        if (j<y_min) y_min = j;
        if (j>y_max) y_max = j;
      }
      else
        c2 = 0;

      *pc++ = c2;
    }
  }
  else
  if (bpp == 3)
  {
    unsigned char *pa = pimage1;
    unsigned char *pb = pimage2;
    unsigned char *pc = ptemp;

    for (j=0; j<h; j++)
    for (i=0; i<w; i++)
    {
      unsigned int c1 = (pa[2]<<16) + (pa[1]<<8) + pa[0];
      unsigned int c2 = (pb[2]<<16) + (pb[1]<<8) + pb[0];
      if (c1 != c2)
      {
        diffnum++;
        if (has_tcolor && c2 == tcolor) over_is_possible = 0;
        if (i<x_min) x_min = i;
        if (i>x_max) x_max = i;
        if (j<y_min) y_min = j;
        if (j>y_max) y_max = j;
      }
      else
        c2 = tcolor;

      memcpy(pc, &c2, 3);
      pa += 3;
      pb += 3;
      pc += 3;
    }
  }
  else
  if (bpp == 4)
  {
    unsigned int *pa = (unsigned int *)pimage1;
    unsigned int *pb = (unsigned int *)pimage2;
    unsigned int *pc = (unsigned int *)ptemp;

    for (j=0; j<h; j++)
    for (i=0; i<w; i++)
    {
      unsigned int c1 = *pa++;
      unsigned int c2 = *pb++;
      if ((c1 != c2) && ((c1>>24) || (c2>>24)))
      {
        diffnum++;
        if ((c2 >> 24) != 0xFF) over_is_possible = 0;
        if (i<x_min) x_min = i;
        if (i>x_max) x_max = i;
        if (j<y_min) y_min = j;
        if (j>y_max) y_max = j;
      }
      else
        c2 = 0;

      *pc++ = c2;
    }
  }

  if (diffnum == 0)
  {
    x0 = y0 = 0;
    w0 = h0 = 1;
  }
  else
  {
    x0 = x_min;
    y0 = y_min;
    w0 = x_max-x_min+1;
    h0 = y_max-y_min+1;
  }

  deflate_rect_op(pimage2, x0, y0, w0, h0, bpp, stride, zbuf_size, n*2);

  if (over_is_possible)
    deflate_rect_op(ptemp, x0, y0, w0, h0, bpp, stride, zbuf_size, n*2+1);
}

void APNGAsm::write_chunk(FILE * f, const char * name, unsigned char * data, unsigned int length)
{
  unsigned int crc = crc32(0, Z_NULL, 0);
  unsigned int len = swap32(length);

  fwrite(&len, 1, 4, f);
  fwrite(name, 1, 4, f);
  crc = crc32(crc, (const Bytef *)name, 4);

  if (memcmp(name, "fdAT", 4) == 0)
  {
    unsigned int seq = swap32(m_next_seq_num++);
    fwrite(&seq, 1, 4, f);
    crc = crc32(crc, (const Bytef *)(&seq), 4);
    length -= 4;
  }

  if (data != NULL && length > 0)
  {
    fwrite(data, 1, length, f);
    crc = crc32(crc, data, length);
  }

  crc = swap32(crc);
  fwrite(&crc, 1, 4, f);
}

void APNGAsm::write_IDATs(FILE * f, int n, unsigned char * data, unsigned int length, unsigned int idat_size)
{
  unsigned int z_cmf = data[0];
  if ((z_cmf & 0x0f) == 8 && (z_cmf & 0xf0) <= 0x70)
  {
    if (length >= 2)
    {
      unsigned int z_cinfo = z_cmf >> 4;
      unsigned int half_z_window_size = 1 << (z_cinfo + 7);
      while (idat_size <= half_z_window_size && half_z_window_size >= 256)
      {
        z_cinfo--;
        half_z_window_size >>= 1;
      }
      z_cmf = (z_cmf & 0x0f) | (z_cinfo << 4);
      if (data[0] != (unsigned char)z_cmf)
      {
        data[0] = (unsigned char)z_cmf;
        data[1] &= 0xe0;
        data[1] += (unsigned char)(0x1f - ((z_cmf << 8) + data[1]) % 0x1f);
      }
    }
  }

  while (length > 0)
  {
    unsigned int ds = length;
    if (ds > 32768)
      ds = 32768;

    if (n == 0)
      write_chunk(f, "IDAT", data, ds);
    else
      write_chunk(f, "fdAT", data, ds+4);

    data += ds;
    length -= ds;
  }
}

// private static object
// TODO to be removed during implementation of actual code
namespace
{
  vector<APNGFrame> tmpFrames;
}

void info_fn(png_structp png_ptr, png_infop info_ptr)
{
  APNGFrame * frame = (APNGFrame *)png_get_progressive_ptr(png_ptr);
  png_set_expand(png_ptr);
  png_set_strip_16(png_ptr);
  png_set_gray_to_rgb(png_ptr);
  png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
  (void)png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
  frame->w = png_get_image_width(png_ptr, info_ptr);
  frame->h = png_get_image_height(png_ptr, info_ptr);
  unsigned int rowbytes = frame->w * 4;

  frame->p = new unsigned char[frame->h * rowbytes];
  frame->rows = new png_bytep[frame->h * sizeof(png_bytep)];
  if (frame->p && frame->rows)
    for (unsigned int j=0; j<frame->h; ++j)
      frame->rows[j] = frame->p + j * rowbytes;
}

void row_fn(png_structp png_ptr, png_bytep new_row, png_uint_32 row_num, int pass)
{
  APNGFrame * frame = (APNGFrame *)png_get_progressive_ptr(png_ptr);
  png_progressive_combine_row(png_ptr, frame->rows[row_num], new_row);
}

void APNGAsm::decode_frame(APNGFrame * frameOut, FramePNG * frameIn)
{
  png_structp png_ptr  = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop   info_ptr = png_create_info_struct(png_ptr);
  if (png_ptr != NULL && info_ptr != NULL && setjmp(png_jmpbuf(png_ptr)) == 0)
  {
    png_set_progressive_read_fn(png_ptr, (void *)frameOut, info_fn, row_fn, NULL);

    for (unsigned int i=0; i<frameIn->chunkSet.size(); ++i)
      png_process_data(png_ptr, info_ptr, frameIn->chunkSet[i].p, frameIn->chunkSet[i].size);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
  }
}

void APNGAsm::compose_frame(unsigned char ** rows_dst, unsigned char ** rows_src, unsigned char bop, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  unsigned int  i, j;
  int u, v, al;

  for (j=0; j<h; j++)
  {
    unsigned char * sp = rows_src[j];
    unsigned char * dp = rows_dst[j+y] + x*4;

    if (bop == 0)
      memcpy(dp, sp, w*4);
    else
    for (i=0; i<w; i++, sp+=4, dp+=4)
    {
      if (sp[3] == 255)
        memcpy(dp, sp, 4);
      else
      if (sp[3] != 0)
      {
        if (dp[3] != 0)
        {
          u = sp[3]*255;
          v = (255-sp[3])*dp[3];
          al = 255*255-(255-sp[3])*(255-dp[3]);
          dp[0] = (sp[0]*u + dp[0]*v)/al;
          dp[1] = (sp[1]*u + dp[1]*v)/al;
          dp[2] = (sp[2]*u + dp[2]*v)/al;
          dp[3] = al/255;
        }
        else
          memcpy(dp, sp, 4);
      }
    }  
  }
}

unsigned int APNGAsm::read_chunk(FILE * f, CHUNK * pChunk)
{
  unsigned int len;
  fread(&len, 4, 1, f);
  pChunk->size = swap32(len) + 12;
  pChunk->p = new unsigned char[pChunk->size];
  unsigned int * pi = (unsigned int *)pChunk->p;
  pi[0] = len;
  fread(pChunk->p + 4, 1, pChunk->size - 4, f);
  return pi[1];
}

void APNGAsm::recalc_crc(CHUNK * pChunk)
{
  unsigned int crc = crc32(0, Z_NULL, 0);
  crc = crc32(crc, pChunk->p + 4, pChunk->size - 8);
  crc = swap32(crc);
  memcpy(pChunk->p + pChunk->size - 4, &crc, 4);
}

void APNGAsm::SavePNG(char * szOut, APNGFrame * frame)
{
  FILE * f;
  if ((f = fopen(szOut, "wb")) != 0)
  {
    png_structp  png_ptr  = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop    info_ptr = png_create_info_struct(png_ptr);
    if (png_ptr != NULL && info_ptr != NULL && setjmp(png_jmpbuf(png_ptr)) == 0)
    {
      png_init_io(png_ptr, f);
      png_set_compression_level(png_ptr, 9);
      if (frame->p && frame->rows)
      {
        png_set_IHDR(png_ptr, info_ptr, frame->w, frame->h, 8, 6, 0, 0, 0);
        png_write_info(png_ptr, info_ptr);
        png_write_image(png_ptr, frame->rows);
        png_write_end(png_ptr, info_ptr);
      }
    }
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(f);
  }
}

const vector<APNGFrame>& APNGAsm::disassemble(const string &filePath)
{
  unsigned int   i, j, id;
  unsigned int   w, h;
  unsigned int * pi;
  unsigned char  header[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  unsigned char  footer[12] = {0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130};
  CHUNK chunk_ihdr;
  CHUNK chunk;
  CHUNK chunk_sign = {8, &header[0]};
  CHUNK chunk_iend = {12, &footer[0]};

  reset();

  FILE * f;
  if ((f = fopen(filePath.c_str(), "rb")) != 0)
  {
    unsigned char sig[8];
    unsigned int  flag_fctl = 0;
    unsigned int  flag_idat = 0;
    unsigned int  len = 1;
    unsigned int  num_frames = 1;

    if (fread(sig, 1, 8, f) == 8 && memcmp(sig, header, 8) == 0)
    {
      id = read_chunk(f, &chunk_ihdr);

      if (id == id_IHDR && chunk_ihdr.size == 25)
      {
        pi = (unsigned int *)chunk_ihdr.p;
        FramePNG frame;
        frame.chunkSet.push_back(chunk_sign);
        frame.chunkSet.push_back(chunk_ihdr);
        frame.w = w = swap32(pi[2]);
        frame.h = h = swap32(pi[3]);
        frame.x = 0;
        frame.y = 0;
        frame.delay_num = 1;
        frame.delay_den = 10;
        frame.dop = 0;
        frame.bop = 0;
        unsigned int rowbytes = w * 4;
        unsigned int imagesize = h * rowbytes;

        APNGFrame frameRaw;
        APNGFrame frameCur;
        APNGFrame frameNext;

        frameCur.w = w;
        frameCur.h = h;
        frameCur.p = new unsigned char[imagesize];
        frameCur.rows = new png_bytep[h * sizeof(png_bytep)];
        if (frameCur.p && frameCur.rows)
          for (j=0; j<h; ++j)
            frameCur.rows[j] = frameCur.p + j * rowbytes;

        while ( !feof(f) )
        {
          id = read_chunk(f, &chunk);

          if (id == id_acTL)
          {
            chunk.flag = 1;
            all_chunks.push_back(chunk);
            pi = (unsigned int *)chunk.p;
            num_frames = swap32(pi[2]);
          }
          else
          if (id == id_fcTL)
          {
            chunk.flag = 1;
            all_chunks.push_back(chunk);
            frame.chunkSet.push_back(chunk_iend);

            if (flag_fctl)
            {
              frameNext.p = new unsigned char[imagesize];
              frameNext.rows = new png_bytep[h * sizeof(png_bytep)];
              if (frameNext.p && frameNext.rows)
                for (j=0; j<h; ++j)
                  frameNext.rows[j] = frameNext.p + j * rowbytes;

              if (frame.dop == 2)
                memcpy(frameNext.p, frameCur.p, imagesize);

              decode_frame(&frameRaw, &frame);
              compose_frame(frameCur.rows, frameRaw.rows, frame.bop, frame.x, frame.y, frame.w, frame.h);
              frameCur.delay_num = frame.delay_num;
              frameCur.delay_den = frame.delay_den;
              frames.push_back(frameCur);

              if (frame.dop != 2)
              {
                memcpy(frameNext.p, frameCur.p, imagesize);
                if (frame.dop == 1)
                  for (j=0; j<frame.h; j++)
                    memset(frameNext.rows[frame.y + j], 0, frame.w * 4);
              }
              frameCur.p = frameNext.p;
              frameCur.rows = frameNext.rows;
            }

            memcpy(chunk_ihdr.p + 8, chunk.p + 12, 8);
            recalc_crc(&chunk_ihdr);
            pi = (unsigned int *)chunk.p;
            frame.chunkSet.clear();
            frame.chunkSet.push_back(chunk_sign);
            frame.chunkSet.push_back(chunk_ihdr);
            frame.w = swap32(pi[3]);
            frame.h = swap32(pi[4]);
            frame.x = swap32(pi[5]);
            frame.y = swap32(pi[6]);
            frame.delay_num = chunk.p[28]*256 + chunk.p[29];
            frame.delay_den = chunk.p[30]*256 + chunk.p[31];
            frame.dop = chunk.p[32];
            frame.bop = chunk.p[33];
            if (!flag_fctl)
            {
              frame.bop = 0;
              if (frame.dop == 2)
                frame.dop = 1;
            }
            flag_fctl = 1;
          }
          else
          if (id == id_IDAT)
          {
            chunk.flag = flag_idat = 1;
            all_chunks.push_back(chunk);

            if (frame.chunkSet.size() == 2)
              for (i=0; i<all_chunks.size(); ++i)
                if (!all_chunks[i].flag)
                  frame.chunkSet.push_back(all_chunks[i]);

            frame.chunkSet.push_back(chunk);
          }
          else
          if (id == id_fdAT)
          {
            chunk.flag = 1;
            all_chunks.push_back(chunk);

            if (frame.chunkSet.size() == 2)
              for (i=0; i<all_chunks.size(); ++i)
                if (!all_chunks[i].flag)
                  frame.chunkSet.push_back(all_chunks[i]);

            chunk.size -= 4;
            chunk.p += 4;
            unsigned int new_size = swap32(chunk.size - 12);
            unsigned int new_id   = id_IDAT;
            memcpy(chunk.p, &new_size, 4);
            memcpy(chunk.p + 4, &new_id, 4);
            recalc_crc(&chunk);

            frame.chunkSet.push_back(chunk);
          }
          else
          if (id == id_IEND)
          {
            chunk.flag = 1;
            all_chunks.push_back(chunk);
            frame.chunkSet.push_back(chunk_iend);

            decode_frame(&frameRaw, &frame);
            compose_frame(frameCur.rows, frameRaw.rows, frame.bop, frame.x, frame.y, frame.w, frame.h);
            frameCur.delay_num = frame.delay_num;
            frameCur.delay_den = frame.delay_den;
            frames.push_back(frameCur);
            break;
          }
          else
          {
            chunk.flag = flag_idat;
            all_chunks.push_back(chunk);

            if (notabc(chunk.p[4]) || notabc(chunk.p[5]) || notabc(chunk.p[6]) || notabc(chunk.p[7]))
              break;
          }
        }
      }
    }
    fclose(f);

    for (i=0; i<all_chunks.size(); ++i)
      delete[] all_chunks[i].p;

    all_chunks.clear();

    delete[] chunk_ihdr.p;
  }
  return frames;
}

//Loads an animation spec from JSON
//Returns a frame vector with the loaded frames
const vector<APNGFrame>& APNGAsm::loadJSONSpec(const string &filePath)
{
  return tmpFrames;
}

//Loads an animation spec from XML
//Returns a frame vector with the loaded frames
const vector<APNGFrame>& APNGAsm::loadXMLSpec(const string &filePath)
{
  return tmpFrames;
}
