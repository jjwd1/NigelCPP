#pragma once
#include "../Module.hpp"
#include "../../DLL/IPC.hpp"

class NigelUI : public Module {
public:
    NigelUI(const std::string& name, const std::string& description, uint32_t states);
    ~NigelUI() override;

    void OnRender() override;

private:
    bool showWindow;
};
