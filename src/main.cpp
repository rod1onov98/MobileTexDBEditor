#include "ImageIO.h"
#include "TextureDb.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
constexpr wchar_t WindowClass[] = L"TexDBEditor.ImGui";
constexpr size_t InvalidIndex = static_cast<size_t>(-1);

enum class Language { English, Russian };
enum class TextId { AppTitle, File, Open, SaveAs, Exit, Texture, Replace, Add, Export, Delete, View, Fit, ActualSize, Language, English, Russian, Search, Name, Size, Alpha, State, Preview, Properties, NoDatabase, OpenHint, NoSelection,Dimensions, AlphaMode, MipMode, Affiliate, Yes, No, Platforms, PreviewSource, TxtRow, NewState, ModifiedState, AffiliateState, OriginalState, Zoom, About, AboutText, UnsavedTitle, UnsavedText, ExistingTitle, ExistingText, DeleteTitle, DeleteText, SaveComplete, OpenDialog, ReplaceDialog, AddDialog, ExportDialog, FolderDialog, StatusReplaced, StatusAdded, StatusDeleted, StatusSaved, StatusExported, ErrorTitle, DecodeUnavailable };

const char* tr(Language lang, TextId id) {
    const bool ru = lang == Language::Russian;
    switch (id) {
    case TextId::AppTitle:
        return ru ? "TexDB Editor" : "TexDB Editor";
    case TextId::File:
        return ru ? "Файл" : "File";
    case TextId::Open:
        return ru ? "Открыть базу..." : "Open database...";
    case TextId::SaveAs:
        return ru ? "Сохранить как..." : "Save As...";
    case TextId::Exit:
        return ru ? "Выход" : "Exit";
    case TextId::Texture:
        return ru ? "Текстура" : "Texture";
    case TextId::Replace:
        return ru ? "Заменить" : "Replace";
    case TextId::Add:
        return ru ? "Добавить PNG" : "Add PNG";
    case TextId::Export:
        return ru ? "Экспорт PNG" : "Export PNG";
    case TextId::Delete:
        return ru ? "Удалить" : "Delete";
    case TextId::View:
        return ru ? "Вид" : "View";
    case TextId::Fit:
        return ru ? "Вписать" : "Fit";
    case TextId::ActualSize:
        return ru ? "100%" : "100%";
    case TextId::Language:
        return ru ? "Язык" : "Language";
    case TextId::English:
        return "English";
    case TextId::Russian:
        return "Русский";
    case TextId::Search:
        return ru ? "Поиск текстур..." : "Search textures...";
    case TextId::Name:
        return ru ? "Имя" : "Name";
    case TextId::Size:
        return ru ? "Размер" : "Size";
    case TextId::Alpha:
        return "Alpha";
    case TextId::State:
        return ru ? "Состояние" : "State";
    case TextId::Preview:
        return ru ? "Предпросмотр" : "Preview";
    case TextId::Properties:
        return ru ? "Свойства" : "Properties";
    case TextId::NoDatabase:
        return ru ? "База текстур не открыта" : "No texture database is open";
    case TextId::OpenHint:
        return ru ? "Откройте texdb txt или перетащите его в окно." : "Open a texdb txt file or drop it here.";
    case TextId::NoSelection:
        return ru ? "Выберите текстуру слева" : "Select a texture on the left";
    case TextId::Dimensions:
        return ru ? "Размер" : "Dimensions";
    case TextId::AlphaMode:
        return ru ? "Режим alpha" : "Alpha mode";
    case TextId::MipMode:
        return ru  ? "Mip mode (TXT)" : "Mip mode (TXT)";
    case TextId::Affiliate:
        return "Affiliate";
    case TextId::Yes:
        return ru ? "да" : "yes";
    case TextId::No:
        return ru ? "нет" : "no";
    case TextId::Platforms:
        return ru ? "Платформы" : "Platforms";
    case TextId::PreviewSource:
        return ru ? "Источник превью" : "Preview source";
    case TextId::TxtRow:
        return ru ? "Строка txt" : "txt row";
    case TextId::NewState:
        return ru  ? "новая" : "new";
    case TextId::ModifiedState:
        return ru ? "изменена" : "modified";
    case TextId::AffiliateState:
        return "affiliate";
    case TextId::OriginalState:
        return ru ? "оригинал" : "original";
    case TextId::Zoom:
        return ru ? "масштаб" : "zoom";
    case TextId::About:
        return ru ? "О программе" : "About";
    case TextId::AboutText:
        if (ru) {
            return "Редактор GTA SA .txt/.dat/.toc texture database.\n"
                   "Изменённые текстуры сохраняются как RGBA8888, mipmode=0.\n"
                   "Исходные файлы Save As не перезаписывает.";
        }

        return "Editor for GTA SA .txt/.dat/.toc texture databases.\n"
               "Modified textures are saved as RGBA8888, mipmode=0.\n"
               "Save As never overwrites source files.";
    case TextId::UnsavedTitle:
        return ru ? "Несохранённые изменения" : "Unsaved changes";
    case TextId::UnsavedText:
        return ru ? "В текущей базе есть несохранённые изменения. Отбросить их?" : "There are unsaved edits in the current database. Discard them?";
    case TextId::ExistingTitle:
        return ru ? "Текстура уже существует" : "Existing texture";
    case TextId::ExistingText:
        return ru ? "Текстура уже существует. Заменить её?" : "The texture already exists. Replace it?";
    case TextId::DeleteTitle:
        return ru  ? "Удаление текстуры" : "Delete texture";
    case TextId::DeleteText:
        return ru ? "Удалить выбранную текстуру из пересобранной базы?" : "Delete the selected texture from the rebuilt database?";
    case TextId::SaveComplete:
        return ru ? "сохранение завершено" : "save complete";
    case TextId::OpenDialog:
        return ru ? "Открыть TXT базы текстур" : "Open texture database TXT";
    case TextId::ReplaceDialog:
        return ru ? "Заменить выбранную текстуру" : "Replace selected texture";
    case TextId::AddDialog:
        return ru ? "Добавить новую текстуру" : "Add new texture";
    case TextId::ExportDialog:
        return ru ? "Экспорт текстуры в PNG" : "Export texture to PNG";
    case TextId::FolderDialog:
        return ru ? "Выберите папку для пересобранной базы" : "Choose output folder for rebuilt database";
    case TextId::StatusReplaced:
        return ru ? "Текстура заменена в памяти. Используйте «Сохранить как», чтобы пересобрать базу." : "Texture replaced in memory. Use Save As to rebuild the database.";
    case TextId::StatusAdded:
        return ru ? "Текстура добавлена/обновлена в памяти." : "Texture added/updated in memory.";
    case TextId::StatusDeleted:
        return ru ? "Текстура помечена на удаление. Исходные файлы не изменены." : "Texture marked for deletion. Source files are untouched.";
    case TextId::StatusSaved:
        return ru ? "База успешно пересобрана" : "Database rebuilt successfully";
    case TextId::StatusExported:
        return ru ? "PNG экспортирован" : "PNG exported";
    case TextId::ErrorTitle:
        return ru ? "Ошибка" : "Error";
    case TextId::DecodeUnavailable:
        return ru ? "превью недоступно" : "preview unavailable";
    }
    return "";
}

struct AppState {
    HWND hwnd{};
    Language language{Language::English};
    std::unique_ptr<texdb::TextureDatabase> db;
    std::vector<size_t> visible;
    size_t selected{InvalidIndex};
    std::optional<texdb::ImageRGBA> preview;
    std::string previewSource;
    ComPtr<ID3D11ShaderResourceView> previewSrv;
    char search[256]{};
    bool dirty{};
    bool fitPreview{true};
    float zoom{1.0f};
    std::string status{"Open a texdb txt file to begin"};
};
AppState g;

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;
ComPtr<IDXGISwapChain> g_swapChain;
ComPtr<ID3D11RenderTargetView> g_rtv;

std::wstring widen(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(std::max(length, 0)), L'\0');

    if (length > 0) {
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    }
    return result;
}

std::string narrowUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }

    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(std::max(length, 0)), '\0');

    if (length > 0) {
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length, nullptr, nullptr);
    }
    return result;
}

std::string lowerAsciiUtf8(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch + ('a' - 'A'));
        }
    }
    return text;
}

void errorBox(const std::string& text) {
    const std::wstring title = widen(tr(g.language, TextId::ErrorTitle));
    MessageBoxW(g.hwnd, widen(text).c_str(), title.c_str(), MB_OK | MB_ICONERROR);
}

std::optional<std::filesystem::path> pickOpen(const wchar_t* title, const wchar_t* filter) {
    wchar_t pathBuffer[32768]{};
    OPENFILENAMEW dialog{sizeof(dialog)};

    dialog.hwndOwner = g.hwnd;
    dialog.lpstrTitle = title;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = pathBuffer;
    dialog.nMaxFile = static_cast<DWORD>(std::size(pathBuffer));
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

    if (!GetOpenFileNameW(&dialog)) {
        return std::nullopt;
    }
    return std::filesystem::path(pathBuffer);
}

std::optional<std::filesystem::path> pickSavePng(const std::wstring& suggestedName) {
    wchar_t pathBuffer[32768]{};
    wcsncpy_s(pathBuffer, suggestedName.c_str(), _TRUNCATE);

    const wchar_t filter[] = L"PNG image (*.png)\0*.png\0All files (*.*)\0*.*\0\0";
    const std::wstring title = widen(tr(g.language, TextId::ExportDialog));

    OPENFILENAMEW dialog{sizeof(dialog)};
    dialog.hwndOwner = g.hwnd;
    dialog.lpstrTitle = title.c_str();
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = pathBuffer;
    dialog.nMaxFile = static_cast<DWORD>(std::size(pathBuffer));
    dialog.lpstrDefExt = L"png";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;

    if (!GetSaveFileNameW(&dialog)) {
        return std::nullopt;
    }
    return std::filesystem::path(pathBuffer);
}

std::optional<std::filesystem::path> pickFolder() {
    IFileDialog* dialog = nullptr;
    const HRESULT createResult = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));

    if (FAILED(createResult)) {
        return std::nullopt;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

    const std::wstring title = widen(tr(g.language, TextId::FolderDialog));
    dialog->SetTitle(title.c_str());

    if (FAILED(dialog->Show(g.hwnd))) {
        dialog->Release();
        return std::nullopt;
    }

    IShellItem* item = nullptr;
    if (FAILED(dialog->GetResult(&item)) || !item) {
        dialog->Release();
        return std::nullopt;
    }

    dialog->Release();

    PWSTR rawPath = nullptr;
    const HRESULT pathResult = item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath);
    item->Release();

    if (FAILED(pathResult) || !rawPath) {
        return std::nullopt;
    }

    std::filesystem::path result(rawPath);
    CoTaskMemFree(rawPath);
    return result;
}

void releasePreview() {
    g.previewSrv.Reset();
    g.preview.reset();
    g.previewSource.clear();
}

void uploadPreviewTexture() {
    g.previewSrv.Reset();

    if (!g.preview || !g.preview->valid()) {
        return;
    }

    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = g.preview->width;
    textureDesc.Height = g.preview->height;
    textureDesc.MipLevels = 0;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(g_device->CreateTexture2D(&textureDesc, nullptr, &texture))) {
        return;
    }

    g_context->UpdateSubresource(texture.Get(), 0, nullptr, g.preview->pixels.data(), g.preview->width * 4, 0);

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
    viewDesc.Format = textureDesc.Format;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MostDetailedMip = 0;
    viewDesc.Texture2D.MipLevels = static_cast<UINT>(-1);

    if (SUCCEEDED(g_device->CreateShaderResourceView(texture.Get(), &viewDesc, &g.previewSrv))) {
        g_context->GenerateMips(g.previewSrv.Get());
    }
}

void rebuildVisible() {
    g.visible.clear();

    if (!g.db) {
        return;
    }

    const std::string searchText = lowerAsciiUtf8(g.search);

    for (size_t index = 0; index < g.db->entries().size(); ++index) {
        const auto& entry = g.db->entries()[index];

        if (entry.deleted) {
            continue;
        }

        if (!searchText.empty() &&
            lowerAsciiUtf8(entry.name).find(searchText) == std::string::npos) {
            continue;
        }

        g.visible.push_back(index);
    }
}

void selectTexture(size_t index) {
    if (!g.db || index >= g.db->entries().size()) {
        return;
    }

    g.selected = index;
    releasePreview();

    try {
        g.preview = g.db->decode(index, &g.previewSource);
        uploadPreviewTexture();
    }
    catch (const std::exception& ex) {
        g.previewSource = std::string(tr(g.language, TextId::DecodeUnavailable)) + ": " + ex.what();
    }
}

bool confirmDiscard() {
    if (!g.dirty) {
        return true;
    }

    const std::wstring text = widen(tr(g.language, TextId::UnsavedText));
    const std::wstring title = widen(tr(g.language, TextId::UnsavedTitle));

    return MessageBoxW(g.hwnd, text.c_str(), title.c_str(), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
}

void openDatabase(const std::filesystem::path& path) {
    if (!confirmDiscard()) {
        return;
    }

    try {
        auto database = std::make_unique<texdb::TextureDatabase>(texdb::TextureDatabase::Open(path));

        g.status = database->validationSummary();
        g.db = std::move(database);
        g.dirty = false;
        g.selected = InvalidIndex;
        g.search[0] = 0;

        releasePreview();
        rebuildVisible();

        if (!g.visible.empty()) {
            selectTexture(g.visible.front());
        }
    }
    catch (const std::exception& ex) {
        errorBox(ex.what());
    }
}

void commandOpen() {
    const wchar_t filter[] =  L"texture database (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";
    const std::wstring title = widen(tr(g.language, TextId::OpenDialog));

    if (auto path = pickOpen(title.c_str(), filter)) {
        openDatabase(*path);
    }
}

void commandReplace() {
    if (!g.db || g.selected == InvalidIndex) {
        return;
    }

    const wchar_t filter[] = L"Images (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All files (*.*)\0*.*\0\0";
    const std::wstring title = widen(tr(g.language, TextId::ReplaceDialog));
    const auto path = pickOpen(title.c_str(), filter);

    if (!path) {
        return;
    }

    try {
        g.db->replace(g.selected, imageio::LoadRGBA(*path));
        g.dirty = true;

        rebuildVisible();
        selectTexture(g.selected);

        g.status = tr(g.language, TextId::StatusReplaced);
    }
    catch (const std::exception& ex) {
        errorBox(ex.what());
    }
}

void commandAdd(const std::filesystem::path* droppedPath = nullptr) {
    if (!g.db) {
        return;
    }

    std::optional<std::filesystem::path> path;

    if (droppedPath) {
        path = *droppedPath;
    }
    else {
        const wchar_t filter[] = L"Images (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All files (*.*)\0*.*\0\0";
        const std::wstring title = widen(tr(g.language, TextId::AddDialog));
        path = pickOpen(title.c_str(), filter);
    }

    if (!path) {
        return;
    }

    try {
        auto image = imageio::LoadRGBA(*path);
        const std::string textureName = narrowUtf8(path->stem().wstring());
        const int existingIndex = g.db->findByName(textureName);
        size_t selectedIndex = 0;

        if (existingIndex >= 0) {
            const std::wstring text = widen(tr(g.language, TextId::ExistingText));
            const std::wstring title = widen(tr(g.language, TextId::ExistingTitle));

            const int result = MessageBoxW(g.hwnd, text.c_str(), title.c_str(), MB_YESNO | MB_ICONQUESTION);

            if (result != IDYES) {
                return;
            }

            selectedIndex = static_cast<size_t>(existingIndex);
            g.db->replace(selectedIndex, std::move(image));
        }
        else {
            selectedIndex = g.db->add(textureName, std::move(image));
        }

        g.dirty = true;
        rebuildVisible();
        selectTexture(selectedIndex);
        g.status = tr(g.language, TextId::StatusAdded);
    }
    catch (const std::exception& ex) {
        errorBox(ex.what());
    }
}

void commandDelete() {
    if (!g.db || g.selected == InvalidIndex) {
        return;
    }

    const std::wstring text = widen(tr(g.language, TextId::DeleteText));
    const std::wstring title = widen(tr(g.language, TextId::DeleteTitle));

    const int result = MessageBoxW(g.hwnd, text.c_str(), title.c_str(), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

    if (result != IDYES) {
        return;
    }

    g.db->erase(g.selected);
    g.dirty = true;
    g.selected = InvalidIndex;

    releasePreview();
    rebuildVisible();

    if (!g.visible.empty()) {
        selectTexture(g.visible.front());
    }

    g.status = tr(g.language, TextId::StatusDeleted);
}

void commandExport() {
    if (!g.db || g.selected == InvalidIndex) {
        return;
    }

    try {
        auto image = g.db->decode(g.selected);
        const std::wstring suggestedName = widen(g.db->entries()[g.selected].name) + L".png";
        const auto outputPath = pickSavePng(suggestedName);

        if (!outputPath) {
            return;
        }

        imageio::SavePNG(*outputPath, image);
        g.status = std::string(tr(g.language, TextId::StatusExported)) + ": " + narrowUtf8(outputPath->wstring());
    }
    catch (const std::exception& ex) {
        errorBox(ex.what());
    }
}

void commandSaveAs() {
    if (!g.db) {
        return;
    }

    const auto folder = pickFolder();
    if (!folder) {
        return;
    }

    try {
        g.db->saveAs(*folder);
        g.dirty = false;
        g.status = std::string(tr(g.language, TextId::StatusSaved)) + ": " + narrowUtf8(folder->wstring());

        const std::wstring title = widen(tr(g.language, TextId::SaveComplete));
        MessageBoxW(g.hwnd, widen(g.status).c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
    }
    catch (const std::exception& ex) {
        errorBox(ex.what());
    }
}

void applyBlackTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 0.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 5.0f;

    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(9.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);
    style.CellPadding = ImVec2(7.0f, 5.0f);

    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.039f, 0.047f, 1.0f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.052f, 0.057f, 0.068f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.045f, 0.050f, 0.060f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.15f, 0.17f, 0.20f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.075f, 0.082f, 0.096f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.12f, 0.15f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.12f, 0.14f, 0.18f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.075f, 0.082f, 0.096f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.11f, 0.34f, 0.58f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.10f, 0.43f, 0.76f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.08f, 0.20f, 0.34f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.10f, 0.30f, 0.52f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.10f, 0.40f, 0.70f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.16f, 0.57f, 1.0f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.16f, 0.57f, 1.0f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.28f, 0.67f, 1.0f, 1.0f);
    colors[ImGuiCol_Separator] = ImVec4(0.14f, 0.16f, 0.20f, 1.0f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.065f, 0.072f, 0.085f, 1.0f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.018f);
    colors[ImGuiCol_Text] = ImVec4(0.93f, 0.94f, 0.96f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.51f, 0.57f, 1.0f);
}

const char* stateText(size_t index) {
    const auto& entry = g.db->entries()[index];

    if (entry.originalIndex < 0) {
        return tr(g.language, TextId::NewState);
    }

    if (entry.replacement) {
        return tr(g.language, TextId::ModifiedState);
    }

    if (g.db->isAffiliate(index)) {
        return tr(g.language, TextId::AffiliateState);
    }

    return tr(g.language, TextId::OriginalState);
}

void drawChecker(ImDrawList* drawList, ImVec2 position, ImVec2 size) {
    constexpr float tileSize = 16.0f;
    const ImU32 firstColor = IM_COL32(43, 46, 53, 255);
    const ImU32 secondColor = IM_COL32(31, 34, 40, 255);
    const int columns = static_cast<int>(std::ceil(size.x / tileSize));
    const int rows = static_cast<int>(std::ceil(size.y / tileSize));

    drawList->PushClipRect(position, ImVec2(position.x + size.x, position.y + size.y), true);

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            const ImVec2 minPoint(position.x + x * tileSize, position.y + y * tileSize);
            const ImVec2 maxPoint(std::min(minPoint.x + tileSize, position.x + size.x), std::min(minPoint.y + tileSize, position.y + size.y));
            const ImU32 color = ((x + y) & 1) ? firstColor : secondColor;
            drawList->AddRectFilled(minPoint, maxPoint, color);
        }
    }
    drawList->PopClipRect();
}

void drawProperties() {
    if (!g.db || g.selected == InvalidIndex || g.selected >= g.db->entries().size()) {
        ImGui::TextDisabled("%s", tr(g.language, TextId::NoSelection));
        return;
    }

    const auto& entry = g.db->entries()[g.selected];

    auto drawRow = [](const char* label, const std::string& value) {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(135.0f);
        ImGui::TextWrapped("%s", value.c_str());
    };

    drawRow(tr(g.language, TextId::Name), entry.name);
    drawRow(tr(g.language, TextId::Dimensions), std::to_string(g.db->widthOf(g.selected)) + " x " + std::to_string(g.db->heightOf(g.selected)));
    drawRow(tr(g.language, TextId::AlphaMode), std::to_string(g.db->alphaModeOf(g.selected)));
    drawRow(tr(g.language, TextId::MipMode), g.db->mipModeOf(g.selected) ? "1" : "0");
    drawRow(tr(g.language, TextId::Affiliate), g.db->isAffiliate(g.selected) ? tr(g.language, TextId::Yes) : tr(g.language, TextId::No));
    drawRow(tr(g.language, TextId::State), stateText(g.selected));

    std::string platforms;
    for (const auto& platform : g.db->platforms()) {
        if (!platforms.empty()) {
            platforms += ", ";
        }
        platforms += platform.tag;
    }

    drawRow(tr(g.language, TextId::Platforms), platforms);
    drawRow(tr(g.language, TextId::PreviewSource), g.previewSource.empty() ? "-" : g.previewSource);

    ImGui::SeparatorText(tr(g.language, TextId::TxtRow));
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(entry.line.c_str());
    ImGui::PopTextWrapPos();
}

void drawPreview() {
    if (!g.preview || !g.previewSrv) {
        const TextId message = g.db ? TextId::NoSelection : TextId::OpenHint;
        ImGui::TextDisabled("%s", tr(g.language, message));
        return;
    }

    ImGui::Checkbox(tr(g.language, TextId::Fit), &g.fitPreview);
    ImGui::SameLine();

    if (ImGui::Button(tr(g.language, TextId::ActualSize))) {
        g.fitPreview = false;
        g.zoom = 1.0f;
    }

    if (!g.fitPreview) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        ImGui::SliderFloat(tr(g.language, TextId::Zoom), &g.zoom, 0.1f, 8.0f, "%.2fx", ImGuiSliderFlags_Logarithmic);
    }

    ImGui::Separator();
    ImGui::BeginChild("##preview_scroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

    const ImVec2 available = ImGui::GetContentRegionAvail();
    float scale = g.zoom;

    if (g.fitPreview) {
        const float scaleX = available.x / static_cast<float>(g.preview->width);
        const float scaleY = available.y / static_cast<float>(g.preview->height);
        scale = std::min(scaleX, scaleY);
    }

    scale = std::clamp(scale, 0.01f, 8.0f);
    const ImVec2 imageSize(g.preview->width * scale, g.preview->height * scale);

    if (g.fitPreview) {
        if (imageSize.x < available.x) {
            const float offsetX = (available.x - imageSize.x) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        }

        if (imageSize.y < available.y) {
            const float offsetY = (available.y - imageSize.y) * 0.5f;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
        }
    }

    const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
    drawChecker(ImGui::GetWindowDrawList(), imagePosition, imageSize);

    ImGui::Image(ImTextureRef(reinterpret_cast<ImTextureID>(g.previewSrv.Get())), imageSize);

    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsItemHovered() &&
        !g.fitPreview &&
        io.KeyCtrl &&
        std::abs(io.MouseWheel) > 0.0f) {
        const float wheelScale = io.MouseWheel > 0.0f ? 1.12f : 0.89f;
        g.zoom = std::clamp(g.zoom * wheelScale, 0.1f, 8.0f);
    }

    ImGui::EndChild();
}

void drawMenuBar() {
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu(tr(g.language, TextId::File))) {
        if (ImGui::MenuItem(tr(g.language, TextId::Open), "Ctrl+O")) {
            commandOpen();
        }

        if (ImGui::MenuItem(tr(g.language, TextId::SaveAs), "Ctrl+Shift+S", false, g.db != nullptr)) {
            commandSaveAs();
        }

        ImGui::Separator();

        if (ImGui::MenuItem(tr(g.language, TextId::Exit))) {
            PostMessageW(g.hwnd, WM_CLOSE, 0, 0);
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(tr(g.language, TextId::Texture))) {
        const bool hasSelection = g.db && g.selected != InvalidIndex;

        if (ImGui::MenuItem(tr(g.language, TextId::Replace), "Ctrl+R", false, hasSelection)) {
            commandReplace();
        }
        if (ImGui::MenuItem(tr(g.language, TextId::Add), "Ctrl+I", false, g.db != nullptr)) {
            commandAdd();
        }
        if (ImGui::MenuItem(tr(g.language, TextId::Export), "Ctrl+E", false, hasSelection)) {
            commandExport();
        }
        if (ImGui::MenuItem(tr(g.language, TextId::Delete), "Del", false, hasSelection)) {
            commandDelete();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(tr(g.language, TextId::Language))) {
        if (ImGui::MenuItem(tr(g.language, TextId::English), nullptr, g.language == Language::English)) {
            g.language = Language::English;
        }

        if (ImGui::MenuItem(tr(g.language, TextId::Russian), nullptr, g.language == Language::Russian)) {
            g.language = Language::Russian;
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("?")) {
        if (ImGui::MenuItem(tr(g.language, TextId::About))) {
            const std::wstring text = widen(tr(g.language, TextId::AboutText));
            const std::wstring title = widen(tr(g.language, TextId::About));
            MessageBoxW(g.hwnd, text.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
        }

        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

void drawTextureTable(ImVec2 tableSize) {
    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("##textures", 4, tableFlags, tableSize)) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(tr(g.language, TextId::Name), ImGuiTableColumnFlags_WidthStretch, 2.1f);
    ImGui::TableSetupColumn(tr(g.language, TextId::Size), 0, 0.9f);
    ImGui::TableSetupColumn(tr(g.language, TextId::Alpha), 0, 0.5f);
    ImGui::TableSetupColumn(tr(g.language, TextId::State), 0, 0.9f);
    ImGui::TableHeadersRow();

    if (g.db) {
        for (size_t index : g.visible) {
            const auto& entry = g.db->entries()[index];

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(static_cast<int>(index));

            if (ImGui::Selectable(entry.name.c_str(), g.selected == index, ImGuiSelectableFlags_SpanAllColumns)) {
                selectTexture(index);
            }

            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u x %u", g.db->widthOf(index), g.db->heightOf(index));

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", g.db->alphaModeOf(index));

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(stateText(index));
        }
    }

    ImGui::EndTable();
}

void drawTextureActions() {
    const bool hasSelection = g.db && g.selected != InvalidIndex;

    ImGui::BeginDisabled(!hasSelection);

    if (ImGui::Button(tr(g.language, TextId::Replace))) {
        commandReplace();
    }

    ImGui::SameLine();
    if (ImGui::Button(tr(g.language, TextId::Export))) {
        commandExport();
    }

    ImGui::SameLine();
    if (ImGui::Button(tr(g.language, TextId::Delete))) {
        commandDelete();
    }

    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!g.db);

    if (ImGui::Button(tr(g.language, TextId::Add))) {
        commandAdd();
    }

    ImGui::EndDisabled();
}

void drawUI() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

    ImGui::Begin("##main", nullptr, windowFlags);
    drawMenuBar();

    constexpr float statusHeight = 28.0f;
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float bodyHeight = std::max(100.0f, available.y - statusHeight - 6.0f);

    const float leftWidth = std::clamp(available.x * 0.34f, 330.0f, 520.0f);
    const float rightWidth = std::clamp(available.x * 0.25f, 280.0f, 390.0f);
    const float centerWidth = std::max(260.0f, available.x - leftWidth - rightWidth - 16.0f);

    ImGui::BeginChild("##left", ImVec2(leftWidth, bodyHeight), true);

    if (ImGui::Button(tr(g.language, TextId::Open))) {
        commandOpen();
    }

    ImGui::SameLine();
    if (ImGui::Button(tr(g.language, TextId::SaveAs)) && g.db) {
        commandSaveAs();
    }

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##search", tr(g.language, TextId::Search), g.search, sizeof(g.search))) {
        rebuildVisible();
    }

    constexpr float buttonsHeight = 38.0f;
    const ImVec2 tableSize(0.0f, ImGui::GetContentRegionAvail().y - buttonsHeight);

    drawTextureTable(tableSize);
    drawTextureActions();

    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##center", ImVec2(centerWidth, bodyHeight), true);
    ImGui::SeparatorText(tr(g.language, TextId::Preview));
    drawPreview();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##right", ImVec2(0.0f, bodyHeight), true);
    ImGui::SeparatorText(tr(g.language, TextId::Properties));
    drawProperties();
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextDisabled("%s", g.status.c_str());
    ImGui::End();
}

void createRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;

    if (SUCCEEDED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &g_rtv);
    }
}

void cleanupRenderTarget() {
    g_rtv.Reset();
}

bool createDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL selectedFeatureLevel{};

    const HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, static_cast<UINT>(std::size(featureLevels)), D3D11_SDK_VERSION, &swapChainDesc, &g_swapChain, &g_device, &selectedFeatureLevel, &g_context);

    if (FAILED(result)) {
        return false;
    }

    createRenderTarget();
    return true;
}

void handleDropFiles(WPARAM wParam) {
    HDROP drop = reinterpret_cast<HDROP>(wParam);
    wchar_t pathBuffer[32768]{};

    if (DragQueryFileW(drop, 0, pathBuffer, static_cast<UINT>(std::size(pathBuffer)))) {
        std::filesystem::path path(pathBuffer);
        std::wstring extension = path.extension().wstring();

        std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t ch) {return static_cast<wchar_t>(towlower(ch));});

        if (extension == L".txt") {
            openDatabase(path);
        }
        else if (g.db) {
            commandAdd(&path);
        }
    }

    DragFinish(drop);
}

bool handleShortcut(WPARAM key) {
    const bool ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (ctrlPressed && key == 'O') {
        commandOpen();
        return true;
    }
    if (ctrlPressed && key == 'R') {
        commandReplace();
        return true;
    }
    if (ctrlPressed && key == 'I') {
        commandAdd();
        return true;
    }
    if (ctrlPressed && key == 'E') {
        commandExport();
        return true;
    }
    if (ctrlPressed && shiftPressed && key == 'S') {
        commandSaveAs();
        return true;
    }
    if (key == VK_DELETE && !ImGui::GetIO().WantTextInput) {
        commandDelete();
        return true;
    }

    return false;
}

LRESULT WINAPI wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam)) {
        return true;
    }

    switch (message) {
    case WM_SIZE:
        if (g_device && wParam != SIZE_MINIMIZED) {
            cleanupRenderTarget();
            g_swapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            createRenderTarget();
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) {
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (handleShortcut(wParam)) {
            return 0;
        }
        break;

    case WM_DROPFILES:
        handleDropFiles(wParam);
        return 0;

    case WM_CLOSE:
        if (!confirmDiscard()) {
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImFontConfig fontConfig{};
    fontConfig.OversampleH = 3;
    fontConfig.OversampleV = 2;
    fontConfig.PixelSnapH = false;

    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17.0f, &fontConfig, io.Fonts->GetGlyphRangesCyrillic());

    if (!font) {
        io.Fonts->AddFontDefault();
    }
}

void openCommandLineDatabase(PWSTR commandLine) {
    if (!commandLine || !*commandLine) {
        return;
    }

    std::wstring path = commandLine;

    if (path.size() > 1 && path.front() == L'\"' && path.back() == L'\"') {
        path = path.substr(1, path.size() - 2);
    }

    if (std::filesystem::exists(path)) {
        openDatabase(path);
    }
}

void renderFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    drawUI();

    ImGui::Render();

    const float clearColor[4] = {
        0.025f,
        0.028f,
        0.034f,
        1.0f
    };

    g_context->OMSetRenderTargets(1, g_rtv.GetAddressOf(), nullptr);
    g_context->ClearRenderTargetView(g_rtv.Get(), clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swapChain->Present(1, 0);
}

void shutdownApplication(HINSTANCE instance, HRESULT comResult) {
    releasePreview();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    cleanupRenderTarget();
    g_swapChain.Reset();
    g_context.Reset();
    g_device.Reset();

    if (IsWindow(g.hwnd)) {
        DestroyWindow(g.hwnd);
    }

    UnregisterClassW(WindowClass, instance);

    if (SUCCEEDED(comResult)) {
        CoUninitialize();
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand) {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = wndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClass;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExW(&windowClass);

    g.hwnd = CreateWindowExW(0, WindowClass, L"TexDB Editor", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1480, 900, nullptr, nullptr, instance, nullptr);

    if (!g.hwnd || !createDevice(g.hwnd)) {
        return 1;
    }

    DragAcceptFiles(g.hwnd, TRUE);
    ShowWindow(g.hwnd, showCommand);
    UpdateWindow(g.hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    loadFonts();
    applyBlackTheme();

    ImGui_ImplWin32_Init(g.hwnd);
    ImGui_ImplDX11_Init(g_device.Get(), g_context.Get());

    openCommandLineDatabase(commandLine);

    bool done = false;
    while (!done) {
        MSG message{};

        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);

            if (message.message == WM_QUIT) {
                done = true;
            }
        }

        if (done) {
            break;
        }

        renderFrame();
    }

    shutdownApplication(instance, comResult);
    return 0;
}
