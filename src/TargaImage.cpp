///////////////////////////////////////////////////////////////////////////////
//
//      TargaImage.cpp                          Author:     Stephen Chenney
//                                              Modified:   Eric McDaniel
//                                              Date:       Fall 2004
//
//      Implementation of TargaImage methods.  You must implement the image
//  modification functions.
//
///////////////////////////////////////////////////////////////////////////////

#include "Globals.h"
#include "TargaImage.h"
#include "libtarga.h"
#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <math.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <map>

using namespace std;

// constants
const int           RED = 0;                // red channel
const int           GREEN = 1;                // green channel
const int           BLUE = 2;                // blue channel
const unsigned char BACKGROUND[3] = { 0, 0, 0 };      // background color


// Computes n choose s, efficiently
double Binomial(int n, int s)
{
	double        res;

	res = 1;
	for (int i = 1; i <= s; i++)
		res = (n - i + 1) * res / i;

	return res;
}// Binomial


///////////////////////////////////////////////////////////////////////////////
//
//      Constructor.  Initialize member variables.
//
///////////////////////////////////////////////////////////////////////////////
TargaImage::TargaImage() : width(0), height(0), data(NULL)
{}// TargaImage

///////////////////////////////////////////////////////////////////////////////
//
//      Constructor.  Initialize member variables.
//
///////////////////////////////////////////////////////////////////////////////
TargaImage::TargaImage(int w, int h) : width(w), height(h)
{
	data = new unsigned char[width * height * 4];
	ClearToBlack();
}// TargaImage



///////////////////////////////////////////////////////////////////////////////
//
//      Constructor.  Initialize member variables to values given.
//
///////////////////////////////////////////////////////////////////////////////
TargaImage::TargaImage(int w, int h, unsigned char* d)
{
	int i;

	width = w;
	height = h;
	data = new unsigned char[width * height * 4];

	for (i = 0; i < width * height * 4; i++)
		data[i] = d[i];
}// TargaImage

///////////////////////////////////////////////////////////////////////////////
//
//      Copy Constructor.  Initialize member to that of input
//
///////////////////////////////////////////////////////////////////////////////
TargaImage::TargaImage(const TargaImage& image)
{
	width = image.width;
	height = image.height;
	data = NULL;
	if (image.data != NULL) {
		data = new unsigned char[width * height * 4];
		memcpy(data, image.data, sizeof(unsigned char) * width * height * 4);
	}
}


///////////////////////////////////////////////////////////////////////////////
//
//      Destructor.  Free image memory.
//
///////////////////////////////////////////////////////////////////////////////
TargaImage::~TargaImage()
{
	if (data)
		delete[] data;
}// ~TargaImage


///////////////////////////////////////////////////////////////////////////////
//
//      Converts an image to RGB form, and returns the rgb pixel data - 24 
//  bits per pixel. The returned space should be deleted when no longer 
//  required.
//
///////////////////////////////////////////////////////////////////////////////
unsigned char* TargaImage::To_RGB(void)
{
	unsigned char* rgb = new unsigned char[width * height * 3];
	int		    i, j;

	if (!data)
		return NULL;

	// Divide out the alpha
	for (i = 0; i < height; i++)
	{
		int in_offset = i * width * 4;
		int out_offset = i * width * 3;

		for (j = 0; j < width; j++)
		{
			RGBA_To_RGB(data + (in_offset + j * 4), rgb + (out_offset + j * 3));
		}
	}

	return rgb;
}// TargaImage


///////////////////////////////////////////////////////////////////////////////
//
//      Save the image to a targa file. Returns 1 on success, 0 on failure.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Save_Image(const char* filename)
{
	TargaImage* out_image = Reverse_Rows();

	if (!out_image)
		return false;

	if (!tga_write_raw(filename, width, height, out_image->data, TGA_TRUECOLOR_32))
	{
		cout << "TGA Save Error: %s\n", tga_error_string(tga_get_last_error());
		return false;
	}

	delete out_image;

	return true;
}// Save_Image


///////////////////////////////////////////////////////////////////////////////
//
//      Load a targa image from a file.  Return a new TargaImage object which 
//  must be deleted by caller.  Return NULL on failure.
//
///////////////////////////////////////////////////////////////////////////////
TargaImage* TargaImage::Load_Image(char* filename)
{
	unsigned char* temp_data;
	TargaImage* temp_image;
	TargaImage* result;
	int		        width, height;

	if (!filename)
	{
		cout << "No filename given." << endl;
		return NULL;
	}// if

	temp_data = (unsigned char*)tga_load(filename, &width, &height, TGA_TRUECOLOR_32);
	if (!temp_data)
	{
		cout << "TGA Error: %s\n", tga_error_string(tga_get_last_error());
		width = height = 0;
		return NULL;
	}
	temp_image = new TargaImage(width, height, temp_data);
	free(temp_data);

	result = temp_image->Reverse_Rows();

	delete temp_image;

	return result;
}// Load_Image


///////////////////////////////////////////////////////////////////////////////
//
//      Convert image to grayscale.  Red, green, and blue channels should all 
//  contain grayscale value.  Alpha channel shoould be left unchanged.  Return
//  success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::To_Grayscale()
{
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			double I = 0;
			I += data[(i * width + j) * 4] * 0.299;//R
			I += data[(i * width + j) * 4 + 1] * 0.587;//G
			I += data[(i * width + j) * 4 + 2] * 0.114;//B

			data[(i * width + j) * 4] = I; //R
			data[(i * width + j) * 4 + 1] = I; //G
			data[(i * width + j) * 4 + 2] = I; //B
		}
	}
	return true;
}// To_Grayscale


///////////////////////////////////////////////////////////////////////////////
//
//  Convert the image to an 8 bit image using uniform quantization.  Return 
//  success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Quant_Uniform()
{
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			data[(i * width + j) * 4] &= 0xE0;//R
			data[(i * width + j) * 4 + 1] &= 0xE0;//G
			data[(i * width + j) * 4 + 2] &= 0xC0;//B 
		}
	}
	return true;
}// Quant_Uniform


///////////////////////////////////////////////////////////////////////////////
//
//      Convert the image to an 8 bit image using populosity quantization.  
//  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Quant_Populosity()
{
	struct RGB
	{
		unsigned char red;
		unsigned char green;
		unsigned char blue;

		bool operator<(const RGB& p2) const
		{
			if (this->red < p2.red)
			{
				return true;
			}
			else if (this->red == p2.red)
			{
				if (this->green < p2.green)
				{
					return true;
				}
				else if (this->green == p2.green)
				{
					if (this->blue < p2.blue)
					{
						return true;
					}
					else
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		bool operator>(const RGB& p2) const
		{
			if (this->red > p2.red)
			{
				return true;
			}
			else if (this->red == p2.red)
			{
				if (this->green > p2.green)
				{
					return true;
				}
				else if (this->green == p2.green)
				{
					if (this->blue > p2.blue)
					{
						return true;
					}
					else
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}

		bool operator==(const RGB& p2) const
		{
			if ((this->red == p2.red) && (this->green == p2.green) && (this->blue == p2.blue))
			{
				return true;
			}
			else
			{
				return false;
			}
		}

	};

	map<RGB, unsigned int>list;
	vector<pair<RGB, unsigned int>> sortList;

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			RGB temp;

			temp.red = data[(i * width + j) * 4] & 0xF8;
			temp.green = data[(i * width + j) * 4 + 1] & 0xF8;
			temp.blue = data[(i * width + j) * 4 + 2] & 0xF8;

			if (list.find(temp) != list.end()) //found
			{
				list[temp]++;
			}
			else //not found
			{
				list[temp] = 1;
			}
		}
	}

	for (const auto& v : list)
	{
		sortList.push_back(v);
	}

	sort(sortList.begin(), sortList.end(), [](const pair<RGB, unsigned int>& p1, const pair<RGB, unsigned int>& p2) {return p1.second > p2.second; });

	sortList.erase(sortList.begin() + 256, sortList.end());

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			bool FoundClosestColor = false;
			RGB thisColor{ data[(i * width + j) * 4] ,data[(i * width + j) * 4 + 1] ,data[(i * width + j) * 4 + 2] };
			RGB closestColor;
			double minDistance = INFINITY;

			for (int k = 0; k < 256; k++)
			{
				double distance = 0;

				distance += (double(sortList[k].first.red) - double(thisColor.red)) * (double(sortList[k].first.red) - double(thisColor.red));
				distance += (double(sortList[k].first.green) - double(thisColor.green)) * (double(sortList[k].first.green) - double(thisColor.green));
				distance += (double(sortList[k].first.blue) - double(thisColor.blue)) * (double(sortList[k].first.blue) - double(thisColor.blue));

				if (distance < minDistance)
				{
					minDistance = distance;
					closestColor = sortList[k].first;
				}
			}


			data[(i * width + j) * 4] = closestColor.red;
			data[(i * width + j) * 4 + 1] = closestColor.green;
			data[(i * width + j) * 4 + 2] = closestColor.blue;

		}
	}

	return true;
}// Quant_Populosity


///////////////////////////////////////////////////////////////////////////////
//
//      Dither the image using a threshold of 1/2.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Dither_Threshold()
{
	if (this->To_Grayscale())
	{
		for (int i = 0; i < height; i++)
		{
			for (int j = 0; j < width; j++)
			{
				if (data[(i * width + j) * 4] > 127)
				{
					data[(i * width + j) * 4] = 255;
					data[(i * width + j) * 4 + 1] = 255;
					data[(i * width + j) * 4 + 2] = 255;
				}
				else
				{
					data[(i * width + j) * 4] = 0;
					data[(i * width + j) * 4 + 1] = 0;
					data[(i * width + j) * 4 + 2] = 0;
				}
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}// Dither_Threshold


///////////////////////////////////////////////////////////////////////////////
//
//      Dither image using random dithering.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Dither_Random()
{
	if (this->To_Grayscale())
	{
		for (int i = 0; i < height; i++)
		{
			for (int j = 0; j < width; j++)
			{
				int temp = rand() % 103 - 51 + data[(i * width + j) * 4];

				if (temp > 127)
				{
					temp = 255;
				}
				else
				{
					temp = 0;
				}

				data[(i * width + j) * 4] = temp;
				data[(i * width + j) * 4 + 1] = temp;
				data[(i * width + j) * 4 + 2] = temp;
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}// Dither_Random


///////////////////////////////////////////////////////////////////////////////
//
//      Perform Floyd-Steinberg dithering on the image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Dither_FS()
{
	if (this->To_Grayscale())
	{
		vector<vector<float>> new_data;  //left index :y , right index :x

		for (int y = 0; y < height; y++)  //transform 
		{
			vector<float> temp;

			for (int x = 0; x < width; x++)
			{
				temp.push_back(data[(y * width + x) * 4] / 255.0);
			}
			new_data.push_back(temp);
		}

		for (int y = 0; y < height; y++)
		{
			float e; //error

			if (y % 2 == 0)
			{
				for (int x = 0; x < width; x++)
				{
					if (new_data[y][x] > 0.5)
					{
						e = new_data[y][x] - 1;
						new_data[y][x] = 1;
					}
					else
					{
						e = new_data[y][x];
						new_data[y][x] = 0;
					}

					if ((y + 1) < height)
					{
						if ((x - 1) >= 0)
						{
							new_data[y + 1][x - 1] += e * (3.0 / 16.0);
						}
						if ((x + 1) < width)
						{
							new_data[y + 1][x + 1] += e * (1.0 / 16.0);
						}

						new_data[y + 1][x] += e * (5.0 / 16.0);
					}
					if ((x + 1) < width)
					{
						new_data[y][x + 1] += e * (7.0 / 16.0);
					}
				}
			}
			else
			{
				for (int x = width - 1; x >= 0; x--)
				{
					if (new_data[y][x] > 0.5)
					{
						e = new_data[y][x] - 1;
						new_data[y][x] = 1;
					}
					else
					{
						e = new_data[y][x];
						new_data[y][x] = 0;
					}

					if ((y + 1) < height)
					{
						if ((x - 1) >= 0)
						{
							new_data[y + 1][x - 1] += e * (1.0 / 16.0);
						}
						if ((x + 1) < width)
						{
							new_data[y + 1][x + 1] += e * (3.0 / 16.0);
						}

						new_data[y + 1][x] += e * (5.0 / 16.0);
					}

					if ((x - 1) >= 0)
					{
						new_data[y][x - 1] += e * (7.0 / 16.0);
					}
				}
			}
		}

		for (int y = 0; y < height; y++)  //restore
		{
			for (int x = 0; x < width; x++)
			{
				data[(y * width + x) * 4] = new_data[y][x] * 255;
				data[(y * width + x) * 4 + 1] = new_data[y][x] * 255;
				data[(y * width + x) * 4 + 2] = new_data[y][x] * 255;
			}
		}

		return true;
	}
	else
	{
		return false;
	}

}// Dither_FS


///////////////////////////////////////////////////////////////////////////////
//
//      Dither the image while conserving the average brightness.  Return 
//  success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Dither_Bright()
{
	if (this->To_Grayscale())
	{
		unsigned long long sum = 0;
		unsigned long long table[256] = { 0 };

		for (int i = 0; i < height; i++)
		{
			for (int j = 0; j < width; j++)
			{
				sum += data[(i * width + j) * 4];
				table[data[(i * width + j) * 4]]++;
			}
		}

		sum /= 255;

		unsigned long long adder = 0;
		int threshold = 255;
		for (threshold = 255; threshold >= 0; threshold--)
		{
			adder += table[threshold];
			if (adder >= sum)
			{
				break;
			}

		}

		for (int i = 0; i < height; i++)
		{
			for (int j = 0; j < width; j++)
			{
				if (data[(i * width + j) * 4] >= threshold)
				{
					data[(i * width + j) * 4] = 255;
					data[(i * width + j) * 4 + 1] = 255;
					data[(i * width + j) * 4 + 2] = 255;
				}
				else
				{
					data[(i * width + j) * 4] = 0;
					data[(i * width + j) * 4 + 1] = 0;
					data[(i * width + j) * 4 + 2] = 0;
				}
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}// Dither_Bright


///////////////////////////////////////////////////////////////////////////////
//
//      Perform clustered differing of the image.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Dither_Cluster()
{
	if (this->To_Grayscale())
	{
		const double matrix[4][4] = {
										{0.7059,0.0588,0.4706,0.1765},
										{0.3529,0.9412,0.7647,0.5294},
										{0.5882,0.8235,0.8824,0.2941},
										{0.2353,0.4118,0.1176,0.6471}
		};

		for (int i = 0; i < height; i++)
		{
			for (int j = 0; j < width; j++)
			{
				if (data[(i * width + j) * 4] >= (matrix[j % 4][i % 4] * 255))
				{
					data[(i * width + j) * 4] = 255;
					data[(i * width + j) * 4 + 1] = 255;
					data[(i * width + j) * 4 + 2] = 255;
				}
				else
				{
					data[(i * width + j) * 4] = 0;
					data[(i * width + j) * 4 + 1] = 0;
					data[(i * width + j) * 4 + 2] = 0;
				}
			}
		}

		return true;
	}
	else
	{
		return false;
	}

}// Dither_Cluster


///////////////////////////////////////////////////////////////////////////////
//
//  Convert the image to an 8 bit image using Floyd-Steinberg dithering over
//  a uniform quantization - the same quantization as in Quant_Uniform.
//  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Dither_Color()
{
	vector<vector<float>> red_data, green_data, blue_data;  //left index :y , right index :x

	for (int y = 0; y < height; y++)  //transform
	{
		vector<float> temp_1, temp_2, temp_3;

		for (int x = 0; x < width; x++)
		{
			temp_1.push_back(data[(y * width + x) * 4] / 255.0);
			temp_2.push_back(data[(y * width + x) * 4 + 1] / 255.0);
			temp_3.push_back(data[(y * width + x) * 4 + 2] / 255.0);
		}
		red_data.push_back(temp_1);
		green_data.push_back(temp_2);
		blue_data.push_back(temp_3);
	}

	for (int y = 0; y < height; y++)
	{
		float e_1, e_2, e_3; //error
		float old_red, old_green, old_blue;

		if (y % 2 == 0)
		{
			for (int x = 0; x < width; x++)
			{
				old_red = red_data[y][x];
				old_green = green_data[y][x];
				old_blue = blue_data[y][x];

				if (red_data[y][x] < 18 / 255.0)  //red
					red_data[y][x] = 0;
				else if ((red_data[y][x] < 54.5 / 255.0))
					red_data[y][x] = 36.0 / 255.0;
				else if ((red_data[y][x] < 91.0 / 255.0))
					red_data[y][x] = 73.0 / 255.0;
				else if ((red_data[y][x] < 127.5 / 255.0))
					red_data[y][x] = 109.0 / 255.0;
				else if ((red_data[y][x] < 164.0 / 255.0))
					red_data[y][x] = 146.0 / 255.0;
				else if ((red_data[y][x] < 200.5 / 255.0))
					red_data[y][x] = 182.0 / 255.0;
				else if ((red_data[y][x] < 237.0 / 255.0))
					red_data[y][x] = 219.0 / 255.0;
				else if (red_data[y][x] >= 237.0 / 255.0)
					red_data[y][x] = 1;

				e_1 = old_red - red_data[y][x]; //red error

				if (green_data[y][x] < 18.0 / 255.0)  //green
					green_data[y][x] = 0;
				else if ((green_data[y][x] < 54.5 / 255.0))
					green_data[y][x] = 36.0 / 255.0;
				else if ((green_data[y][x] < 91.0 / 255.0))
					green_data[y][x] = 73.0 / 255.0;
				else if ((green_data[y][x] < 127.5 / 255.0))
					green_data[y][x] = 109.0 / 255.0;
				else if ((green_data[y][x] < 164.0 / 255.0))
					green_data[y][x] = 146.0 / 255.0;
				else if ((green_data[y][x] < 200.5 / 255.0))
					green_data[y][x] = 182.0 / 255.0;
				else if ((green_data[y][x] < 237.0 / 255.0))
					green_data[y][x] = 219.0 / 255.0;
				else if (green_data[y][x] >= 237.0 / 255.0)
					green_data[y][x] = 1;

				e_2 = old_green - green_data[y][x];  //green error

				if (blue_data[y][x] < 42.5 / 255.0) //blue
					blue_data[y][x] = 0;
				else if ((blue_data[y][x] < 127.5 / 255.0))
					blue_data[y][x] = 85.0 / 255.0;
				else if ((blue_data[y][x] < 212.5 / 255.0))
					blue_data[y][x] = 170.0 / 255.0;
				else if (blue_data[y][x] >= (212.5 / 255.0))
					blue_data[y][x] = 1;

				e_3 = old_blue - blue_data[y][x];  //blue error



				if ((y + 1) < height)
				{
					if ((x - 1) >= 0)
					{
						red_data[y + 1][x - 1] += e_1 * (3.0 / 16.0);
						green_data[y + 1][x - 1] += e_2 * (3.0 / 16.0);
						blue_data[y + 1][x - 1] += e_3 * (3.0 / 16.0);
					}
					if ((x + 1) < width)
					{
						red_data[y + 1][x + 1] += e_1 * (1.0 / 16.0);
						green_data[y + 1][x + 1] += e_2 * (1.0 / 16.0);
						blue_data[y + 1][x + 1] += e_3 * (1.0 / 16.0);
					}

					red_data[y + 1][x] += e_1 * (5.0 / 16.0);
					green_data[y + 1][x] += e_2 * (5.0 / 16.0);
					blue_data[y + 1][x] += e_3 * (5.0 / 16.0);
				}
				if ((x + 1) < width)
				{
					red_data[y][x + 1] += e_1 * (7.0 / 16.0);
					green_data[y][x + 1] += e_2 * (7.0 / 16.0);
					blue_data[y][x + 1] += e_3 * (7.0 / 16.0);
				}
			}
		}
		else
		{
			for (int x = width - 1; x >= 0; x--)
			{
				old_red = red_data[y][x];
				old_green = green_data[y][x];
				old_blue = blue_data[y][x];

				if (red_data[y][x] < 18.0 / 255.0)  //red
					red_data[y][x] = 0;
				else if ((red_data[y][x] < 54.5 / 255.0))
					red_data[y][x] = 36.0 / 255.0;
				else if ((red_data[y][x] < 91.0 / 255.0))
					red_data[y][x] = 73.0 / 255.0;
				else if ((red_data[y][x] < 127.5 / 255.0))
					red_data[y][x] = 109.0 / 255.0;
				else if ((red_data[y][x] < 164.0 / 255.0))
					red_data[y][x] = 146.0 / 255.0;
				else if ((red_data[y][x] < 200.5 / 255.0))
					red_data[y][x] = 182.0 / 255.0;
				else if ((red_data[y][x] < 237.0 / 255.0))
					red_data[y][x] = 219.0 / 255.0;
				else if (red_data[y][x] >= 237.0 / 255.0)
					red_data[y][x] = 1;

				e_1 = old_red - red_data[y][x]; //red error

				if (green_data[y][x] < 18.0 / 255.0)  //green
					green_data[y][x] = 0;
				else if ((green_data[y][x] < 54.5 / 255.0))
					green_data[y][x] = 36.0 / 255.0;
				else if ((green_data[y][x] < 91.0 / 255.0))
					green_data[y][x] = 73.0 / 255.0;
				else if ((green_data[y][x] < 127.5 / 255.0))
					green_data[y][x] = 109.0 / 255.0;
				else if ((green_data[y][x] < 164.0 / 255.0))
					green_data[y][x] = 146.0 / 255.0;
				else if ((green_data[y][x] < 200.5 / 255.0))
					green_data[y][x] = 182.0 / 255.0;
				else if ((green_data[y][x] < 237.0 / 255.0))
					green_data[y][x] = 219.0 / 255.0;
				else if (green_data[y][x] >= 237.0 / 255.0)
					green_data[y][x] = 1;

				e_2 = old_green - green_data[y][x];  //green error

				if (blue_data[y][x] < 42.5 / 255.0) //blue
					blue_data[y][x] = 0;
				else if ((blue_data[y][x] < 127.5 / 255.0))
					blue_data[y][x] = 85.0 / 255.0;
				else if ((blue_data[y][x] < 212.5 / 255.0))
					blue_data[y][x] = 170.0 / 255.0;
				else if (blue_data[y][x] >= (212.5 / 255.0))
					blue_data[y][x] = 1;

				e_3 = old_blue - blue_data[y][x];  //blue error

				if ((y + 1) < height)
				{
					if ((x - 1) >= 0)
					{
						red_data[y + 1][x - 1] += e_1 * (1.0 / 16.0);
						green_data[y + 1][x - 1] += e_2 * (1.0 / 16.0);
						blue_data[y + 1][x - 1] += e_3 * (1.0 / 16.0);
					}
					if ((x + 1) < width)
					{
						red_data[y + 1][x + 1] += e_1 * (3.0 / 16.0);
						green_data[y + 1][x + 1] += e_2 * (3.0 / 16.0);
						blue_data[y + 1][x + 1] += e_3 * (3.0 / 16.0);
					}

					red_data[y + 1][x] += e_1 * (5.0 / 16.0);
					green_data[y + 1][x] += e_2 * (5.0 / 16.0);
					blue_data[y + 1][x] += e_3 * (5.0 / 16.0);
				}
				if ((x - 1) >= 0)
				{
					red_data[y][x - 1] += e_1 * (7.0 / 16.0);
					green_data[y][x - 1] += e_2 * (7.0 / 16.0);
					blue_data[y][x - 1] += e_3 * (7.0 / 16.0);
				}
			}
		}
	}

	for (int y = 0; y < height; y++)  //restore
	{
		for (int x = 0; x < width; x++)
		{
			data[(y * width + x) * 4] = red_data[y][x] * 255;
			data[(y * width + x) * 4 + 1] = green_data[y][x] * 255;
			data[(y * width + x) * 4 + 2] = blue_data[y][x] * 255;
		}
	}

	return true;
}// Dither_Color


///////////////////////////////////////////////////////////////////////////////
//
//      Composite the current image over the given image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Comp_Over(TargaImage* pImage)
{
	if (width != pImage->width || height != pImage->height)
	{
		cout << "Comp_Over: Images not the same size\n";
		return false;
	}

	ClearToBlack();
	return false;
}// Comp_Over


///////////////////////////////////////////////////////////////////////////////
//
//      Composite this image "in" the given image.  See lecture notes for 
//  details.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Comp_In(TargaImage* pImage)
{
	if (width != pImage->width || height != pImage->height)
	{
		cout << "Comp_In: Images not the same size\n";
		return false;
	}

	ClearToBlack();
	return false;
}// Comp_In


///////////////////////////////////////////////////////////////////////////////
//
//      Composite this image "out" the given image.  See lecture notes for 
//  details.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Comp_Out(TargaImage* pImage)
{
	if (width != pImage->width || height != pImage->height)
	{
		cout << "Comp_Out: Images not the same size\n";
		return false;
	}

	ClearToBlack();
	return false;
}// Comp_Out


///////////////////////////////////////////////////////////////////////////////
//
//      Composite current image "atop" given image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Comp_Atop(TargaImage* pImage)
{
	if (width != pImage->width || height != pImage->height)
	{
		cout << "Comp_Atop: Images not the same size\n";
		return false;
	}

	ClearToBlack();
	return false;
}// Comp_Atop


///////////////////////////////////////////////////////////////////////////////
//
//      Composite this image with given image using exclusive or (XOR).  Return
//  success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Comp_Xor(TargaImage* pImage)
{
	if (width != pImage->width || height != pImage->height)
	{
		cout << "Comp_Xor: Images not the same size\n";
		return false;
	}

	ClearToBlack();
	return false;
}// Comp_Xor


///////////////////////////////////////////////////////////////////////////////
//
//      Calculate the difference bewteen this imag and the given one.  Image 
//  dimensions must be equal.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Difference(TargaImage* pImage)
{
	if (!pImage)
		return false;

	if (width != pImage->width || height != pImage->height)
	{
		cout << "Difference: Images not the same size\n";
		return false;
	}// if

	for (int i = 0; i < width * height * 4; i += 4)
	{
		unsigned char        rgb1[3];
		unsigned char        rgb2[3];

		RGBA_To_RGB(data + i, rgb1);
		RGBA_To_RGB(pImage->data + i, rgb2);

		data[i] = abs(rgb1[0] - rgb2[0]);
		data[i + 1] = abs(rgb1[1] - rgb2[1]);
		data[i + 2] = abs(rgb1[2] - rgb2[2]);
		data[i + 3] = 255;
	}

	return true;
}// Difference


///////////////////////////////////////////////////////////////////////////////
//
//      Perform 5x5 box filter on this image.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Filter_Box()
{
	unsigned char* newImage = new unsigned char[width * height * 4];
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int sum_red = 0, sum_green = 0, sum_blue = 0;

			for (int j = -2; j <= 2; j++)
			{
				for (int i = -2; i <= 2; i++)
				{
					int surround_x, surround_y;
					if (((x + i) < 0) || ((x + i) >= width))
					{
						surround_x = x - i;
					}
					else
					{
						surround_x = x + i;
					}

					if (((y + j) < 0) || ((y + j) >= height))
					{
						surround_y = y - j;
					}
					else
					{
						surround_y = y + j;
					}

					sum_red += data[(surround_y * width + surround_x) * 4];
					sum_green += data[(surround_y * width + surround_x) * 4 + 1];
					sum_blue += data[(surround_y * width + surround_x) * 4 + 2];
				}
			}
			newImage[(y * width + x) * 4] = sum_red / 25;
			newImage[(y * width + x) * 4 + 1] = sum_green / 25;
			newImage[(y * width + x) * 4 + 2] = sum_blue / 25;
			newImage[(y * width + x) * 4 + 3] = 255;
		}
	}

	memcpy(data, newImage, sizeof(unsigned char) * width * height * 4);
	delete[] newImage;

	return true;
}// Filter_Box


///////////////////////////////////////////////////////////////////////////////
//
//      Perform 5x5 Bartlett filter on this image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Filter_Bartlett()
{
	unsigned char* newImage = new unsigned char[width * height * 4];
	const int matrix[5][5] = {
								{1,2,3,2,1},
								{2,4,6,4,2},
								{3,6,9,6,3},
								{2,4,6,4,2},
								{1,2,3,2,1}
	};
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int sum_red = 0, sum_green = 0, sum_blue = 0;

			for (int j = -2; j <= 2; j++)
			{
				for (int i = -2; i <= 2; i++)
				{
					int surround_x, surround_y;
					if (((x + i) < 0) || ((x + i) >= width))
					{
						surround_x = x - i;
					}
					else
					{
						surround_x = x + i;
					}

					if (((y + j) < 0) || ((y + j) >= height))
					{
						surround_y = y - j;
					}
					else
					{
						surround_y = y + j;
					}

					sum_red += data[(surround_y * width + surround_x) * 4] * matrix[j + 2][i + 2];
					sum_green += data[(surround_y * width + surround_x) * 4 + 1] * matrix[j + 2][i + 2];
					sum_blue += data[(surround_y * width + surround_x) * 4 + 2] * matrix[j + 2][i + 2];
				}
			}
			newImage[(y * width + x) * 4] = sum_red / 81;
			newImage[(y * width + x) * 4 + 1] = sum_green / 81;
			newImage[(y * width + x) * 4 + 2] = sum_blue / 81;
			newImage[(y * width + x) * 4 + 3] = 255;
		}
	}

	memcpy(data, newImage, sizeof(unsigned char) * width * height * 4);
	delete[] newImage;

	return true;
}// Filter_Bartlett


///////////////////////////////////////////////////////////////////////////////
//
//      Perform 5x5 Gaussian filter on this image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Filter_Gaussian()
{
	unsigned char* newImage = new unsigned char[width * height * 4];
	const int matrix[5][5] = {
								{1,4,6,4,1},
								{4,16,24,16,4},
								{6,24,36,24,6},
								{4,16,24,16,4},
								{1,4,6,4,1}
	};
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int sum_red = 0, sum_green = 0, sum_blue = 0;

			for (int j = -2; j <= 2; j++)
			{
				for (int i = -2; i <= 2; i++)
				{
					int surround_x, surround_y;
					if (((x + i) < 0) || ((x + i) >= width))
					{
						surround_x = x - i;
					}
					else
					{
						surround_x = x + i;
					}

					if (((y + j) < 0) || ((y + j) >= height))
					{
						surround_y = y - j;
					}
					else
					{
						surround_y = y + j;
					}

					sum_red += data[(surround_y * width + surround_x) * 4] * matrix[j + 2][i + 2];
					sum_green += data[(surround_y * width + surround_x) * 4 + 1] * matrix[j + 2][i + 2];
					sum_blue += data[(surround_y * width + surround_x) * 4 + 2] * matrix[j + 2][i + 2];
				}
			}
			newImage[(y * width + x) * 4] = sum_red / 256;
			newImage[(y * width + x) * 4 + 1] = sum_green / 256;
			newImage[(y * width + x) * 4 + 2] = sum_blue / 256;
			newImage[(y * width + x) * 4 + 3] = 255;
		}
	}

	memcpy(data, newImage, sizeof(unsigned char) * width * height * 4);
	delete[] newImage;

	return true;
}// Filter_Gaussian

///////////////////////////////////////////////////////////////////////////////
//
//      Perform NxN Gaussian filter on this image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////

bool TargaImage::Filter_Gaussian_N(unsigned int N)
{
	ClearToBlack();
	return false;
}// Filter_Gaussian_N


///////////////////////////////////////////////////////////////////////////////
//
//      Perform 5x5 edge detect (high pass) filter on this image.  Return 
//  success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Filter_Edge()
{
	ClearToBlack();
	return false;
}// Filter_Edge


///////////////////////////////////////////////////////////////////////////////
//
//      Perform a 5x5 enhancement filter to this image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Filter_Enhance()
{
	ClearToBlack();
	return false;
}// Filter_Enhance


///////////////////////////////////////////////////////////////////////////////
//
//      Run simplified version of Hertzmann's painterly image filter.
//      You probably will want to use the Draw_Stroke funciton and the
//      Stroke class to help.
// Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::NPR_Paint()
{
	ClearToBlack();
	return false;
}



///////////////////////////////////////////////////////////////////////////////
//
//      Halve the dimensions of this image.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Half_Size()
{
	unsigned char* newImage = new unsigned char[(width / 2) * (height / 2) * 4];
	float matrix[3][3] = {
							{0.0625,0.125,0.0625},
							{0.125,0.25,0.125},
							{0.0625,0.125,0.0625}
	};

	for (int y = 0; y < height / 2; y++)
	{
		for (int x = 0; x < width / 2; x++)
		{
			float sum_red = 0, sum_green = 0, sum_blue = 0;

			for (int j = -1; j <= 1; j++)
			{
				for (int i = -1; i <= 1; i++)
				{
					int surround_x, surround_y;
					if (((2 * x + i) < 0) || ((2 * x + i) >= width))
					{
						surround_x = 2 * x - i;
					}
					else
					{
						surround_x = 2 * x + i;
					}

					if (((2 * y + j) < 0) || ((2 * y + j) >= height))
					{
						surround_y = 2 * y - j;
					}
					else
					{
						surround_y = 2 * y + j;
					}

					sum_red += data[(surround_y * width + surround_x) * 4] * matrix[i + 1][j + 1];
					sum_green += data[(surround_y * width + surround_x) * 4 + 1] * matrix[i + 1][j + 1];
					sum_blue += data[(surround_y * width + surround_x) * 4 + 2] * matrix[i + 1][j + 1];
				}
			}

			newImage[(y * (width / 2) + x) * 4] = sum_red;
			newImage[(y * (width / 2) + x) * 4 + 1] = sum_green;
			newImage[(y * (width / 2) + x) * 4 + 2] = sum_blue;
			newImage[(y * (width / 2) + x) * 4 + 3] = 255;
		}
	}

	width /= 2;
	height /= 2;
	delete[] data;
	data = newImage;

	return true;
}// Half_Size


///////////////////////////////////////////////////////////////////////////////
//
//      Double the dimensions of this image.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Double_Size()
{
	unsigned char* newImage = new unsigned char[(width * 2) * (height * 2) * 4];
	float matrix_even[3][3] = {
							{0.0625,0.125,0.0625},
							{0.125,0.25,0.125},
							{0.0625,0.125,0.0625}
	};

	float matrix_odd[4][4] = {
							{0.015625,0.046875,0.046875,0.015625},
							{0.046875,0.140625,0.140625,0.046875},
							{0.046875,0.140625,0.140625,0.046875},
							{0.015625,0.046875,0.046875,0.015625}
	};

	float matrix_even_odd[4][3] = {
							{0.03125,0.0625,0.03125},
							{0.09375,0.18755,0.09375},
							{0.09375,0.18755,0.09375},
							{0.03125,0.0625,0.03125}
	};

	for (int y = 0; y < height * 2; y++)
	{
		for (int x = 0; x < width * 2; x++)
		{
			float sum_red = 0, sum_green = 0, sum_blue = 0;
			if ((x % 2 == 0) && (y % 2 == 0))
			{
				for (int j = -1; j <= 1; j++)
				{
					for (int i = -1; i <= 1; i++)
					{
						int surround_x, surround_y;
						if (((x / 2 + i) < 0) || ((x / 2 + i) >= width))
						{
							surround_x = (x / 2) - i;
						}
						else
						{
							surround_x = (x / 2) + i;
						}

						if (((y / 2 + j) < 0) || ((y / 2 + j) >= height))
						{
							surround_y = y / 2 - j;
						}
						else
						{
							surround_y = y / 2 + j;
						}

						sum_red += data[(surround_y * width + surround_x) * 4] * matrix_even[i + 1][j + 1];
						sum_green += data[(surround_y * width + surround_x) * 4 + 1] * matrix_even[i + 1][j + 1];
						sum_blue += data[(surround_y * width + surround_x) * 4 + 2] * matrix_even[i + 1][j + 1];
					}
				}
			}
			else if ((x % 2 == 1) && (y % 2 == 1))
			{
				for (int j = -1; j <= 2; j++)
				{
					for (int i = -1; i <= 2; i++)
					{
						int surround_x, surround_y;
						if (((x / 2 + i) < 0) || ((x / 2 + i) >= width))
						{
							surround_x = (x / 2) - i;
						}
						else
						{
							surround_x = (x / 2) + i;
						}

						if (((y / 2 + j) < 0) || ((y / 2 + j) >= height))
						{
							surround_y = y / 2 - j;
						}
						else
						{
							surround_y = y / 2 + j;
						}

						sum_red += data[(surround_y * width + surround_x) * 4] * matrix_odd[i + 1][j + 1];
						sum_green += data[(surround_y * width + surround_x) * 4 + 1] * matrix_odd[i + 1][j + 1];
						sum_blue += data[(surround_y * width + surround_x) * 4 + 2] * matrix_odd[i + 1][j + 1];
					}
				}
			}
			else if ((x % 2 == 0) && (y % 2 == 1))
			{
				for (int j = -1; j <= 2; j++)
				{
					for (int i = -1; i <= 1; i++)
					{
						int surround_x, surround_y;
						if (((x / 2 + i) < 0) || ((x / 2 + i) >= width))
						{
							surround_x = (x / 2) - i;
						}
						else
						{
							surround_x = (x / 2) + i;
						}

						if (((y / 2 + j) < 0) || ((y / 2 + j) >= height))
						{
							surround_y = y / 2 - j;
						}
						else
						{
							surround_y = y / 2 + j;
						}

						sum_red += data[(surround_y * width + surround_x) * 4] * matrix_even_odd[j + 1][i + 1];
						sum_green += data[(surround_y * width + surround_x) * 4 + 1] * matrix_even_odd[j + 1][i + 1];
						sum_blue += data[(surround_y * width + surround_x) * 4 + 2] * matrix_even_odd[j + 1][i + 1];
					}
				}
			}

			else if ((x % 2 == 1) && (y % 2 == 0))
			{
				for (int j = -1; j <= 1; j++)
				{
					for (int i = -1; i <= 2; i++)
					{
						int surround_x, surround_y;
						if (((x / 2 + i) < 0) || ((x / 2 + i) >= width))
						{
							surround_x = (x / 2) - i;
						}
						else
						{
							surround_x = (x / 2) + i;
						}

						if (((y / 2 + j) < 0) || ((y / 2 + j) >= height))
						{
							surround_y = y / 2 - j;
						}
						else
						{
							surround_y = y / 2 + j;
						}

						sum_red += data[(surround_y * width + surround_x) * 4] * matrix_even_odd[i + 1][j + 1];
						sum_green += data[(surround_y * width + surround_x) * 4 + 1] * matrix_even_odd[i + 1][j + 1];
						sum_blue += data[(surround_y * width + surround_x) * 4 + 2] * matrix_even_odd[i + 1][j + 1];
					}
				}
			}



			newImage[(y * (width * 2) + x) * 4] = sum_red;
			newImage[(y * (width * 2) + x) * 4 + 1] = sum_green;
			newImage[(y * (width * 2) + x) * 4 + 2] = sum_blue;
			newImage[(y * (width * 2) + x) * 4 + 3] = 255;
		}
	}

	width *= 2;
	height *= 2;

	delete[] data;
	data = newImage;

	return true;
}// Double_Size


///////////////////////////////////////////////////////////////////////////////
//
//      Scale the image dimensions by the given factor.  The given factor is 
//  assumed to be greater than one.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Resize(float scale)
{
	ClearToBlack();
	return false;
}// Resize


//////////////////////////////////////////////////////////////////////////////
//
//      Rotate the image clockwise by the given angle.  Do not resize the 
//  image.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Rotate(float angleDegrees)
{
	ClearToBlack();
	return false;
}// Rotate


//////////////////////////////////////////////////////////////////////////////
//
//      Given a single RGBA pixel return, via the second argument, the RGB
//      equivalent composited with a black background.
//
///////////////////////////////////////////////////////////////////////////////
void TargaImage::RGBA_To_RGB(unsigned char* rgba, unsigned char* rgb)
{
	const unsigned char	BACKGROUND[3] = { 0, 0, 0 };

	unsigned char  alpha = rgba[3];

	if (alpha == 0)
	{
		rgb[0] = BACKGROUND[0];
		rgb[1] = BACKGROUND[1];
		rgb[2] = BACKGROUND[2];
	}
	else
	{
		float	alpha_scale = (float)255 / (float)alpha;
		int	val;
		int	i;

		for (i = 0; i < 3; i++)
		{
			val = (int)floor(rgba[i] * alpha_scale);
			if (val < 0)
				rgb[i] = 0;
			else if (val > 255)
				rgb[i] = 255;
			else
				rgb[i] = val;
		}
	}
}// RGA_To_RGB


///////////////////////////////////////////////////////////////////////////////
//
//      Copy this into a new image, reversing the rows as it goes. A pointer
//  to the new image is returned.
//
///////////////////////////////////////////////////////////////////////////////
TargaImage* TargaImage::Reverse_Rows(void)
{
	unsigned char* dest = new unsigned char[width * height * 4];
	TargaImage* result;
	int 	        i, j;

	if (!data)
		return NULL;

	for (i = 0; i < height; i++)
	{
		int in_offset = (height - i - 1) * width * 4;
		int out_offset = i * width * 4;

		for (j = 0; j < width; j++)
		{
			dest[out_offset + j * 4] = data[in_offset + j * 4];
			dest[out_offset + j * 4 + 1] = data[in_offset + j * 4 + 1];
			dest[out_offset + j * 4 + 2] = data[in_offset + j * 4 + 2];
			dest[out_offset + j * 4 + 3] = data[in_offset + j * 4 + 3];
		}
	}

	result = new TargaImage(width, height, dest);
	delete[] dest;
	return result;
}// Reverse_Rows


///////////////////////////////////////////////////////////////////////////////
//
//      Clear the image to all black.
//
///////////////////////////////////////////////////////////////////////////////
void TargaImage::ClearToBlack()
{
	memset(data, 0, width * height * 4);
}// ClearToBlack


///////////////////////////////////////////////////////////////////////////////
//
//      Helper function for the painterly filter; paint a stroke at
// the given location
//
///////////////////////////////////////////////////////////////////////////////
void TargaImage::Paint_Stroke(const Stroke& s) {
	int radius_squared = (int)s.radius * (int)s.radius;
	for (int x_off = -((int)s.radius); x_off <= (int)s.radius; x_off++) {
		for (int y_off = -((int)s.radius); y_off <= (int)s.radius; y_off++) {
			int x_loc = (int)s.x + x_off;
			int y_loc = (int)s.y + y_off;
			// are we inside the circle, and inside the image?
			if ((x_loc >= 0 && x_loc < width && y_loc >= 0 && y_loc < height)) {
				int dist_squared = x_off * x_off + y_off * y_off;
				if (dist_squared <= radius_squared) {
					data[(y_loc * width + x_loc) * 4 + 0] = s.r;
					data[(y_loc * width + x_loc) * 4 + 1] = s.g;
					data[(y_loc * width + x_loc) * 4 + 2] = s.b;
					data[(y_loc * width + x_loc) * 4 + 3] = s.a;
				}
				else if (dist_squared == radius_squared + 1) {
					data[(y_loc * width + x_loc) * 4 + 0] =
						(data[(y_loc * width + x_loc) * 4 + 0] + s.r) / 2;
					data[(y_loc * width + x_loc) * 4 + 1] =
						(data[(y_loc * width + x_loc) * 4 + 1] + s.g) / 2;
					data[(y_loc * width + x_loc) * 4 + 2] =
						(data[(y_loc * width + x_loc) * 4 + 2] + s.b) / 2;
					data[(y_loc * width + x_loc) * 4 + 3] =
						(data[(y_loc * width + x_loc) * 4 + 3] + s.a) / 2;
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//
//      Build a Stroke
//
///////////////////////////////////////////////////////////////////////////////
Stroke::Stroke() {}

///////////////////////////////////////////////////////////////////////////////
//
//      Build a Stroke
//
///////////////////////////////////////////////////////////////////////////////
Stroke::Stroke(unsigned int iradius, unsigned int ix, unsigned int iy,
	unsigned char ir, unsigned char ig, unsigned char ib, unsigned char ia) :
	radius(iradius), x(ix), y(iy), r(ir), g(ig), b(ib), a(ia)
{
}

