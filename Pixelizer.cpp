#include "pch.h"
#include "Pixelizer.h"
#include <vector>
#include <iostream>
#include <fstream>

Pixelizer::Pixelizer(const char* targetImgPath, const  char* colorTablePath, const  char* resultFile, float targetImgScalingFactor, int pixelImgsResolution, float noRepeatRange, int maxRecursions)
{
	this->grayscaleRange = 0.02;
	this->hueRange = 6;
	this->saturationRange = 0.04;
	this->valueRange = 0.04;
	this->resultFile = resultFile;
	this->noRepeatRange = noRepeatRange;
	this->pixelImgsResolution = pixelImgsResolution;
	this->maxRecursions = maxRecursions;
	pixelImgPaths = new std::vector<std::vector<std::string>>;

	targetImg = new CImg<unsigned char>(targetImgPath);
	targetImg->resize((int)targetImg->width() * targetImgScalingFactor, (int)targetImg->height() * targetImgScalingFactor, 1, 3, 3);

	std::cout << "reading file...";
	std::ifstream jsonFile(colorTablePath);
	std::string fileContent;
	if (jsonFile.is_open())
	{
		std::string line = "";
		while (std::getline(jsonFile, line)) {
			fileContent += line;
		}
		jsonFile.close();
	}

	std::cout << "parsing json...";
	colorTable = new std::vector<imgData>;
	json tmpJson = json::parse(fileContent);
	for (int i = 0; i < tmpJson["colorTable"].size(); i++)
	{
		if (tmpJson["colorTable"][i]["category"] == 1)
			continue;
		imgData tmpData;
		tmpData.avr_sat = tmpJson["colorTable"][i]["avr_sat"];
		tmpData.avr_val = tmpJson["colorTable"][i]["avr_val"];
		tmpData.avr_val_score = tmpJson["colorTable"][i]["avr_val_score"];
		try
		{
			tmpData.max_hue = tmpJson["colorTable"][i]["max_hue"];
			tmpData.max_hue_score = tmpJson["colorTable"][i]["max_hue_score"];
		}
		catch (nlohmann::detail::type_error e)
		{
			tmpData.max_hue = 0;
			tmpData.max_hue_score = 0;
		}
		tmpData.category = tmpJson["colorTable"][i]["category"];
		tmpData.file_path = tmpJson["colorTable"][i]["file_path"].get<std::string>();

		colorTable->push_back(tmpData);
	}
	std::cout << std::endl;
}

Pixelizer::~Pixelizer()
{
	delete colorTable;
	delete targetImg;
	delete pixelImgPaths;
}

void Pixelizer::findImageMatches()
{
	pixelImgPaths->resize(targetImg->height());

	int prevProgress = -1;
	bool prevWasHalf = false;
	std::cout << " [__________________________________________________]";
	std::cout << "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";

	for (int y = 0; y < targetImg->height(); y++)
	{
		(*pixelImgPaths)[y].resize(targetImg->width());
		for (int x = 0; x < targetImg->width(); x++)
		{
			int posXY[2] = { x, y };
			int tmpRGB[3] = { (*targetImg)(x, y, 0, 0), (*targetImg)(x, y, 0, 1), (*targetImg)(x, y, 0, 2) };
			(*pixelImgPaths)[y][x] = (*colorTable)[findImageMatch(rgbToHsv(tmpRGB), posXY)].file_path;
			if ((int)((100 * (y * targetImg->width() + x)) / (targetImg->height() * targetImg->width())) > prevProgress)
			{
				std::cout << (prevWasHalf ? "=" : "-\b");
				prevWasHalf = !prevWasHalf;
				prevProgress++;
			}
		}
	}
	std::cout << std::endl;
}

CImg<unsigned char> Pixelizer::createFinalImg()
{
	CImg<unsigned char> finalImg(pixelImgsResolution * targetImg->width(), pixelImgsResolution * targetImg->height(), 1, 3, 0);

	int prevProgress = -1;
	bool prevWasHalf = false;
	std::cout << " [__________________________________________________]";
	std::cout << "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";

	for (int y = 0; y < targetImg->height(); y++)
	{
		for (int x = 0; x < targetImg->width(); x++)
		{
			CImg<unsigned char> tmpImg((*pixelImgPaths)[y][x].c_str());
			if (tmpImg.width() != tmpImg.height()) {
				if (tmpImg.height() > tmpImg.width())
					tmpImg.crop(0, (tmpImg.height() - tmpImg.width()) / 2, tmpImg.width(), (tmpImg.width() + (tmpImg.height() - tmpImg.width()) / 2));
				else
					tmpImg.crop((tmpImg.width() - tmpImg.height()) / 2, 0, (tmpImg.height() + (tmpImg.width() - tmpImg.height()) / 2), tmpImg.height());
			}
			tmpImg.resize(pixelImgsResolution, pixelImgsResolution, 1, 3, 3);
			finalImg.draw_image((int) x * pixelImgsResolution, (int) y * pixelImgsResolution, 0, 0, tmpImg);

			if ((int)((100 * (y * targetImg->width() + x)) / (targetImg->height() * targetImg->width())) > prevProgress)
			{
				std::cout << (prevWasHalf ? "=" : "-\b");
				prevWasHalf = !prevWasHalf;
				prevProgress++;
			}
		}
	}

	finalImg.display();
	finalImg.save(resultFile);
	return finalImg;
}

int Pixelizer::findImageMatch(std::array<float, 3> hsvPixels, int posXY[2], int recursionCount)
{
	std::map<float, int, std::greater<float>>* closeImgs = new std::map<float, int, std::greater<float>>;

	//grayscale
	if (hsvPixels[1] < 0.09 || hsvPixels[2] < 0.04)
	{
		//find all images withing range
		for (int i = 0; i < colorTable->size(); i++)
		{
			if ((*colorTable)[i].category == 2 &&
				abs(hsvPixels[2] - (float)(*colorTable)[i].avr_val) < grayscaleRange &&
				(*colorTable)[i].times_used < 10)
					closeImgs->insert(std::pair<float, int>((*colorTable)[i].avr_val_score, i));
		}

		//if images have been found find the best fitting one
		if (!closeImgs->empty()){
			//stop after too many recursions (the lower maxRecursions is, the faster the matching)
			if (recursionCount > maxRecursions)
			{
				(*colorTable)[closeImgs->begin()->second].times_used += 1;
				(*colorTable)[closeImgs->begin()->second].used_at.push_back({ posXY[0], posXY[1] });
				int tmpOut = closeImgs->begin()->second;
				delete closeImgs;
				return tmpOut;
			}

			//evaluate all found images and return the first best image
			for (std::pair<float, int> valueScore : *closeImgs)
			{
				if ((*colorTable)[valueScore.second].times_used == 0)
				{
					(*colorTable)[valueScore.second].times_used += 1;
					(*colorTable)[valueScore.second].used_at.push_back({ posXY[0], posXY[1] });
					int tmpOut = valueScore.second;
					delete closeImgs;
					return tmpOut;
				}

				//check for distance to same picture
				bool imgUsable = (*colorTable)[valueScore.second].times_used < 7;
				for (int i = 0; i < (*colorTable)[valueScore.second].times_used && imgUsable; i++)
				{
					if (sqrt(pow(posXY[0] - (*colorTable)[valueScore.second].used_at[i][0], 2) +
						pow(posXY[1] - (*colorTable)[valueScore.second].used_at[i][1], 2))
						<= noRepeatRange)
					{
						imgUsable = false;
					}
				}

				if (imgUsable)
				{
					(*colorTable)[valueScore.second].times_used += 1;
					(*colorTable)[valueScore.second].used_at.push_back({ posXY[0], posXY[1] });
					int tmpOut = valueScore.second;
					delete closeImgs;
					return tmpOut;
				}
			}
		}

		//extend range if no images were found
		this->grayscaleRange += 0.01;
		delete closeImgs;
		int out = findImageMatch(hsvPixels, posXY, recursionCount + 1);
		this->grayscaleRange -= 0.01;
		return out;
		
	}
	//colorful pixel :)
	else
	{
		//find all images withing range
		for (int i = 0; i < colorTable->size(); i++)
		{
			if ((*colorTable)[i].category == 0)
			{
				if (distanceOnCircle(hsvPixels[0], (*colorTable)[i].max_hue) < hueRange &&
					abs(hsvPixels[1] - (float)(*colorTable)[i].avr_sat) < saturationRange &&
					abs(hsvPixels[2] - (float)(*colorTable)[i].avr_val) < valueRange)
					closeImgs->insert(std::pair<float, int>((*colorTable)[i].max_hue_score, i));
			}
		}

		//find image where the target hue appears the most often
		if (!closeImgs->empty())
		{
			//stop after too many recursions (the lower maxRecursions is, the faster the matching)
			if (recursionCount > maxRecursions)
			{
				(*colorTable)[closeImgs->begin()->second].times_used += 1;
				(*colorTable)[closeImgs->begin()->second].used_at.push_back({ posXY[0], posXY[1] });
				int tmpOut = closeImgs->begin()->second;
				delete closeImgs;
				return tmpOut;
			}

			//evaluate all found images and return the first best image
			for (std::pair<float, int> hueScore : *closeImgs)
			{
				if ((*colorTable)[hueScore.second].times_used == 0)
				{
					(*colorTable)[hueScore.second].times_used += 1;
					(*colorTable)[hueScore.second].used_at.push_back({ posXY[0], posXY[1] });
					int tmpOut = hueScore.second;
					delete closeImgs;
					return tmpOut;
				}

				//check for distance to same picture
				bool imgUsable = (*colorTable)[hueScore.second].times_used < 7;
				for (int i = 0; i < (*colorTable)[hueScore.second].times_used && imgUsable; i++)
				{
					if (sqrt(pow(posXY[0] - (*colorTable)[hueScore.second].used_at[i][0], 2) +
						pow(posXY[1] - (*colorTable)[hueScore.second].used_at[i][1], 2))
						<= noRepeatRange)
					{
						imgUsable = false;
					}
				}

				if (imgUsable)
				{
					(*colorTable)[hueScore.second].times_used += 1;
					(*colorTable)[hueScore.second].used_at.push_back({ posXY[0], posXY[1] });
					int tmpOut = hueScore.second;
					delete closeImgs;
					return tmpOut;
				}
			}

		}

		//extend range if no images were found
		this->hueRange += 6;
		this->saturationRange += 0.06;
		this->valueRange += 0.06;
		delete closeImgs;
		int out = findImageMatch(hsvPixels, posXY, recursionCount + 1);
		this->hueRange -= 6;
		this->saturationRange -= 0.06;
		this->valueRange -= 0.06;
		return out;
	}



	delete closeImgs;
	return 0;
}

float Pixelizer::distanceOnCircle(float val1, float val2)
{
	int minDistance = abs(val1 - val2);

	return minDistance > 180 ? 360 - minDistance : minDistance;
}

