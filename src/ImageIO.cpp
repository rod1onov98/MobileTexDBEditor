#include "ImageIO.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;

namespace imageio {
namespace {

std::runtime_error hrError(const char* what, HRESULT hr) {
    char buf[128]{};
    sprintf_s(buf, "%s (HRESULT 0x%08lX)", what, static_cast<unsigned long>(hr));
    return std::runtime_error(buf);
}

ComPtr<IWICImagingFactory> factory() {
    ComPtr<IWICImagingFactory> f;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f));
    if (FAILED(hr)) {
        throw hrError("WIC factory creation failed", hr);
    }
    return f;
}

} // namespace

texdb::ImageRGBA LoadRGBA(const std::filesystem::path& file) {
    auto f = factory();
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = f->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) {
        throw hrError("Cannot open image", hr);
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        throw hrError("Cannot decode image frame", hr);
    }

    UINT w = 0, h = 0;
    hr = frame->GetSize(&w, &h);
    if (FAILED(hr) || w == 0 || h == 0) {
        throw hrError("Invalid image dimensions", hr);
    }

    ComPtr<IWICFormatConverter> conv;
    hr = f->CreateFormatConverter(&conv);
    if (FAILED(hr)) {
        throw hrError("Cannot create image converter", hr);
    }

    hr = conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        throw hrError("Cannot convert image to RGBA8", hr);
    }

    texdb::ImageRGBA out;
    out.width = w;
    out.height = h;
    out.pixels.resize(static_cast<std::size_t>(w) * h * 4u);
    const UINT stride = w * 4u;
    const UINT total = static_cast<UINT>(out.pixels.size());
    hr = conv->CopyPixels(nullptr, stride, total, out.pixels.data());
    if (FAILED(hr)) {
        throw hrError("Cannot read image pixels", hr);
    }
    return out;
}

void SavePNG(const std::filesystem::path& file, const texdb::ImageRGBA& image) {
    if (!image.valid()) {
        throw std::runtime_error("Invalid RGBA image");
    }
    auto f = factory();

    ComPtr<IWICStream> stream;
    HRESULT hr = f->CreateStream(&stream);
    if (FAILED(hr)) {
        throw hrError("Cannot create WIC stream", hr);
    }
    hr = stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        throw hrError("Cannot create PNG file", hr);
    }

    ComPtr<IWICBitmapEncoder> encoder;
    hr = f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr)) {
        throw hrError("Cannot create PNG encoder", hr);
    }
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        throw hrError("Cannot initialize PNG encoder", hr);
    }

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(&frame, &props);
    if (FAILED(hr)) {
        throw hrError("Cannot create PNG frame", hr);
    }
    hr = frame->Initialize(props.Get());
    if (FAILED(hr)) {
        throw hrError("Cannot initialize PNG frame", hr);
    }
    hr = frame->SetSize(image.width, image.height);
    if (FAILED(hr)) {
        throw hrError("Cannot set PNG dimensions", hr);
    }

    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppRGBA;
    hr = frame->SetPixelFormat(&fmt);
    if (FAILED(hr)) {
        throw hrError("Cannot set PNG pixel format", hr);
    }

    const UINT stride = image.width * 4u;
    hr = frame->WritePixels(image.height, stride,
                            static_cast<UINT>(image.pixels.size()),
                            const_cast<BYTE*>(image.pixels.data()));
    if (FAILED(hr)) {
        throw hrError("Cannot write PNG pixels", hr);
    }
    hr = frame->Commit();
    if (FAILED(hr)) {
        throw hrError("Cannot finalize PNG frame", hr);
    }
    hr = encoder->Commit();
    if (FAILED(hr)) {
        throw hrError("Cannot finalize PNG", hr);
    }
}

} // namespace imageio
