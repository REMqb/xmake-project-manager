#pragma once

#include <exewrappers/XMakeTools.hpp>
#include <exewrappers/XMakeWrapper.hpp>

#include <utils/id.h>

#include <projectexplorer/kit.h>
#include <projectexplorer/kitmanager.h>

namespace XMakeProjectManager::Internal {
    class XMakeToolKitAspect final: public ProjectExplorer::KitAspect {
        Q_DECLARE_TR_FUNCTIONS(XMakeProjectMnager::Internal::XMakeToolKitAspect)

      public:
        XMakeToolKitAspect();
        ~XMakeToolKitAspect() override;

        XMakeToolKitAspect(XMakeToolKitAspect &&)      = delete;
        XMakeToolKitAspect(const XMakeToolKitAspect &) = delete;

        XMakeToolKitAspect &operator=(XMakeToolKitAspect &&) = delete;
        XMakeToolKitAspect &operator=(const XMakeToolKitAspect &) = delete;

        ProjectExplorer::Tasks validate(const ProjectExplorer::Kit *kit) const override;

        void setup(ProjectExplorer::Kit *kit) override;
        void fix(ProjectExplorer::Kit *kit) override;

        ItemList toUserOutput(const ProjectExplorer::Kit *kit) const override;

        ProjectExplorer::KitAspectWidget *
            createConfigWidget(ProjectExplorer::Kit *kit) const override;

        static void setXMakeTool(ProjectExplorer::Kit *kit, const Utils::Id &id);
        static Utils::Id xmakeToolId(const ProjectExplorer::Kit *kit);

        static decltype(auto) xmakeTool(const ProjectExplorer::Kit *kit);
        static bool isValid(const ProjectExplorer::Kit *kit);
    };
} // namespace XMakeProjectManager::Internal

#include "XMakeToolKitAspect.inl"