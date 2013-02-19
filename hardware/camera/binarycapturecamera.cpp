#include "binarycapturecamera.h"

#include "../projector/graycodepattern.h"

BinaryCaptureCamera::BinaryCaptureCamera(QObject *parent) :
    PixelEncodedCamera(parent),
    inProgress(false)
{
}

bool BinaryCaptureCamera::setBitRange(uint lo, uint hi)
{
    if(lo>=hi) return false;
    loBit = lo;
    hiBit = hi;
}

int BinaryCaptureCamera::grayToBinary(int num)
{
    unsigned int numBits = 8 * sizeof(num);
    unsigned int shift;
    for (shift = 1; shift < numBits; shift = 2 * shift)
    {
        num = num ^ (num >> shift);
    }
    return num;
}

QString BinaryCaptureCamera::describe()
{
    return QString("Binary capture\n bits %1 through %2")
            .arg(loBit)
            .arg(hiBit);
}

bool BinaryCaptureCamera::requestFrame(QCamera::FrameType type)
{
    uint i;
    QProjector *projector = getProjector();

    // we can't handle more than one thing at a time
    // we also can't handle color
    if(inProgress || type==FULL_COLOR)
        return false;

    // do some bookkeeping
    inProgress = true;
    this->type = type;

    // resize the frames buffer
    allocateFrames((hiBit-loBit)*2+2);

    // push the projection patterns to the projector
    for(i=loBit;i<=hiBit;i++){
        projector->queue(new GrayCodePattern(i,true));
        projector->queue(new GrayCodePattern(i,false));
    }

}

void BinaryCaptureCamera::patternProjected(
        QProjector::Pattern *pattern)
{
    GrayCodePattern *gray;
    gray = dynamic_cast<GrayCodePattern*>(pattern);
    QCamera *camera = getCapturingCamera();

    // if it's not Gray code we're not interested
    if(gray==0) return;

    uint bit = gray->getBit();

    // if the bit's out of range we're not interested
    // (boy we're picky)
    if(bit<loBit || bit>hiBit) return;

    currentFrameIndex = getPatternIndex(pattern);
}

int BinaryCaptureCamera::getPatternIndex(QProjector::Pattern *pattern)
{
    GrayCodePattern *gray;
    gray = dynamic_cast<GrayCodePattern*>(pattern);

    // if it's not Gray code we're not interested
    if(gray==0) return -1;

    uint bit = gray->getBit();

    // if the bit's out of range we're not interested
    // (boy we're picky)
    if(bit<loBit || bit>hiBit) return-1;

    return 2*(bit-loBit) + gray->isInverted()?1:0;
}

bool BinaryCaptureCamera::hasCapturedRawFrame(uint bit, bool inv)
{
    if(bit<loBit || bit>hiBit) return false;
    uint idx = (bit-loBit)*2+(inv?1:0);
    return hasCapturedFrame(idx);
}

cv::Mat BinaryCaptureCamera::getRawFrame(uint bit, bool inv)
{
    uint idx = (bit-loBit)*2+(inv?1:0);
    return getCapturedFrame(idx);
}

void BinaryCaptureCamera::compileFrames()
{
    uint bit,x,y;
    cv::Mat encoding = cv::Mat::zeros(
                getResolutionU(),
                getResolutionV(),
                CV_32S);
    cv::Mat img;
    for(bit=loBit;bit<=hiBit;bit++){
        if(!(hasCapturedRawFrame(bit,true)&&
             hasCapturedRawFrame(bit,false))){
            continue;
        }
        img = getRawFrame(bit,false) - getRawFrame(bit,true);
        img.convertTo(img,CV_32S);
        for(x=0;x<img.cols;x++){
            for(y=0;y<img.rows;y++){
                if(img.at<int>(y,x)>0){
                    encoding.at<int>(y,x) += 1<<bit;
                }
            }
        }
    }
    encoding.convertTo(encoding,getOpenCVFlagFromType(type));
    if(capturedAllFrames())
        emit frameCaptured(encoding,type);
    else
        emit intermediateFrame(encoding);
}