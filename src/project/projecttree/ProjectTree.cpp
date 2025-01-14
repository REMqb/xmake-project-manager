// Copyright (C) 2022 Arthur LAURENT <arthur.laurent4@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level of this distribution

#include "ProjectTree.hpp"
#include "utils/filepath.h"

#include <QApplication>

#include <utils/algorithm.h>
#include <utils/utilsicons.h>

#include <projectexplorer/projectexplorerconstants.h>

#include <set>

#include <QLoggingCategory>

namespace XMakeProjectManager::Internal {
    static Q_LOGGING_CATEGORY(xmake_project_tree_log, "qtc.xmake.projecttree", QtDebugMsg);

    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    auto fromXMakeKind(Target::Kind kind) -> ProjectExplorer::ProductType {
        switch (kind) {
            case Target::Kind::BINARY: return ProjectExplorer::ProductType::App;
            case Target::Kind::SHARED:
            case Target::Kind::OBJECT:
            case Target::Kind::HEADERONLY:
            case Target::Kind::STATIC: return ProjectExplorer::ProductType::Lib;
        }

        return ProjectExplorer::ProductType::Other;
    }

    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    auto createVirtualNode(const Utils::FilePath &path, const QString &name)
        -> std::unique_ptr<ProjectExplorer::VirtualFolderNode> {
        if (path.isEmpty()) return nullptr;

        auto node = std::make_unique<ProjectExplorer::VirtualFolderNode>(path);

        node->setPriority(ProjectExplorer::Node::DefaultFolderPriority + 5);
        node->setDisplayName(name);

        return node;
    }

    auto createGroupNode(const Utils::FilePath &path, const QString &name)
        -> std::unique_ptr<ProjectExplorer::VirtualFolderNode> {
        auto node = createVirtualNode(path, name);
        if (!node) return nullptr;

        qCDebug(xmake_project_tree_log)
            << QString { "Group node '%1' %2" }.arg(path.baseName(), node->path().toUserOutput());
        node->setIsSourcesOrHeaders(false);
        node->setListInProject(false);
        node->setIcon(
            ProjectExplorer::DirectoryIcon { ProjectExplorer::Constants::FILEOVERLAY_MODULES });

        return node;
    }

    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    auto createSourceGroupNode(const Utils::FilePath &path, const QString &name)
        -> std::unique_ptr<ProjectExplorer::VirtualFolderNode> {
        auto node = createVirtualNode(path, name);
        if (!node) return nullptr;

        qCDebug(xmake_project_tree_log)
            << QString { "Source Group node '%1' %2" }.arg(name, node->path().toUserOutput());

        node->setIsSourcesOrHeaders(true);
        node->setListInProject(false);
        node->setIcon([] { return QIcon::fromTheme("edit-copy", ::Utils::Icons::COPY.icon()); });

        return node;
    }

    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    auto findOrCreateGroup(XMakeProjectNode *root, const QStringList &group)
        -> ProjectExplorer::VirtualFolderNode * {
        using FolderNodePtr = ProjectExplorer::FolderNode *;

        if (std::empty(group) || group[0] == "." || group[0] == "") return nullptr;

        const auto path = [&group]() {
            auto path = Utils::FilePath {};

            for (const auto &part : group) path = path / part;

            return path;
        }();

        auto *node = root->findNode([&path](auto *node) { return node->filePath() == path; });
        if (node) { return dynamic_cast<ProjectExplorer::VirtualFolderNode *>(node); }

        auto parent =
            FolderNodePtr { findOrCreateGroup(root, path.parentDir().toString().split('/')) };
        if (parent == nullptr) parent = root;

        auto group_node = createGroupNode(path, path.baseName());

        auto ptr = group_node.get();

        parent->addNode(std::move(group_node));

        return ptr;
    }

    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    auto buildTargetSourceTree(ProjectExplorer::VirtualFolderNode *node,
                               const Target::SourceGroupList &sources) -> void {
        auto pixmap =
            QApplication::style()->standardIcon(QStyle::SP_FileIcon).pixmap(QSize { 16, 16 });
        auto cpp_icon = QIcon {};
        cpp_icon.addPixmap(
            Core::FileIconProvider::overlayIcon(pixmap,
                                                QIcon {
                                                    ProjectExplorer::Constants::FILEOVERLAY_CPP }));
        auto c_icon = QIcon {};
        c_icon.addPixmap(
            Core::FileIconProvider::overlayIcon(pixmap,
                                                QIcon {
                                                    ProjectExplorer::Constants::FILEOVERLAY_C }));

        for (const auto &group : sources) {
            for (const auto &filename : group.sources) {
                if (filename.endsWith(".inl") || filename.endsWith(".hpp") ||
                    filename.endsWith(".h") || filename.endsWith(".mpp") ||
                    filename.endsWith(".hxx") || filename.endsWith(".tpp") ||
                    filename.endsWith(".ixx"))
                    continue;

                auto file = Utils::FilePath::fromString(filename).absoluteFilePath();

                qCDebug(xmake_project_tree_log) << "Source node" << file.toUserOutput();
                auto source_node =
                    std::make_unique<ProjectExplorer::FileNode>(file,
                                                                ProjectExplorer::FileType::Source);

                if (file.endsWith(".cpp")) source_node->setIcon(cpp_icon);
                if (file.endsWith(".c")) source_node->setIcon(c_icon);

                node->addNestedNode(std::move(source_node), {}, [](const Utils::FilePath &fn) {
                    qCDebug(xmake_project_tree_log) << "Folder node" << fn;
                    return std::make_unique<ProjectExplorer::FolderNode>(fn);
                });
            }
        }
    }

    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    auto buildTargetModuleTree(ProjectExplorer::VirtualFolderNode *node,
                               const Target::SourceGroupList &sources) -> void {
        auto pixmap =
            QApplication::style()->standardIcon(QStyle::SP_FileIcon).pixmap(QSize { 16, 16 });
        auto cpp_icon = QIcon {};
        cpp_icon.addPixmap(
            Core::FileIconProvider::overlayIcon(pixmap,
                                                QIcon {
                                                    ProjectExplorer::Constants::FILEOVERLAY_CPP }));
        auto nodes = std::vector<std::unique_ptr<ProjectExplorer::FileNode>> {};
        for (const auto &group : sources) {
            for (const auto &filename : group.sources) {
                auto file = Utils::FilePath::fromString(filename).absoluteFilePath();

                qCDebug(xmake_project_tree_log) << "Module node" << filename;
                auto module_node =
                    std::make_unique<ProjectExplorer::FileNode>(file,
                                                                ProjectExplorer::FileType::Source);
                if (file.endsWith(".mpp")) module_node->setIcon(cpp_icon);
                nodes.emplace_back(std::move(module_node));
            }
        }

        node->addNestedNodes(std::move(nodes));

        for (auto *n : node->folderNodes()) n->compress();
    }

    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    auto buildTargetHeaderTree(ProjectExplorer::VirtualFolderNode *node, const QStringList &headers)
        -> void {
        auto pixmap =
            QApplication::style()->standardIcon(QStyle::SP_FileIcon).pixmap(QSize { 16, 16 });
        auto icon = QIcon {};
        icon.addPixmap(
            Core::FileIconProvider::overlayIcon(pixmap,
                                                QIcon {
                                                    ProjectExplorer::Constants::FILEOVERLAY_H }));

        for (const auto &filename : headers) {
            auto file = Utils::FilePath::fromString(filename).absoluteFilePath();

            qCDebug(xmake_project_tree_log) << "Header node" << file.toUserOutput();
            auto header_node =
                std::make_unique<ProjectExplorer::FileNode>(file,
                                                            ProjectExplorer::FileType::Header);
            header_node->setIcon(icon);
            node->addNestedNode(std::move(header_node), {}, [](const Utils::FilePath &fn) {
                qCDebug(xmake_project_tree_log) << "Folder node" << fn;
                return std::make_unique<ProjectExplorer::FolderNode>(fn);
            });
        }
    }

    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    auto buildTargetExternalPackagesTree(const Utils::FilePath &path,
                                         const QString &target_name,
                                         const QStringList &packages,
                                         const QStringList &frameworks)

        -> std::unique_ptr<ProjectExplorer::VirtualFolderNode> {
        auto parent = createVirtualNode(path, "External Packages");
        if (!parent) return nullptr;

        parent->setIcon(QIcon { ProjectExplorer::Constants::FILEOVERLAY_MODULES });
        parent->setIsSourcesOrHeaders(false);
        parent->setListInProject(false);

        for (const auto &dependency_list : { std::cref(packages), std::cref(frameworks) })
            for (const auto &package : dependency_list.get()) {
                auto node =
                    std::make_unique<ProjectExplorer::FileNode>(path / package,
                                                                ProjectExplorer::FileType::Unknown);
                node->setIcon(QIcon { ProjectExplorer::Constants::FILEOVERLAY_MODULES });
                node->setListInProject(false);

                parent->addNode(std::move(node));

                qCDebug(xmake_project_tree_log) << "Package node" << package;
            }

        return parent;
    }

    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    auto addTargetNode(ProjectExplorer::FolderNode *root, const Target &target)
        -> XMakeTargetNode * {
        using XMakeTargetNodePtr = XMakeTargetNode *;
        auto *output_node        = XMakeTargetNodePtr { nullptr };

        auto target_node =
            std::make_unique<XMakeTargetNode>(Utils::FilePath::fromString(target.defined_in)
                                                  .absolutePath(),
                                              target.name,
                                              fromXMakeKind(target.kind));
        qCDebug(xmake_project_tree_log)
            << "Target node " << target.name << " defined in"
            << Utils::FilePath::fromString(target.defined_in).toUserOutput();
        target_node->setDisplayName(target.name);

        output_node = target_node.get();

        root->addNode(std::move(target_node));

        return output_node;
    }

    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    ProjectTree::ProjectTree() = default;

    ////////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    auto ProjectTree::buildTree(const Utils::FilePath &src_dir,
                                const Utils::FilePath &project_dir,
                                const TargetsList &targets,
                                const std::vector<Utils::FilePath> &bs_files)
        -> std::unique_ptr<XMakeProjectNode> {
        using FolderNodePtr = ProjectExplorer::FolderNode *;
        auto target_paths   = std::set<Utils::FilePath> {};

        auto project_node = std::make_unique<XMakeProjectNode>(src_dir);

        qCDebug(xmake_project_tree_log) << targets.size() << "target(s) found";
        for (const auto &target : targets) {
            auto sources = target.sources;

            auto root = FolderNodePtr { findOrCreateGroup(project_node.get(), target.group) };
            if (root == nullptr) root = project_node.get();

            auto *parent = addTargetNode(root, target);

            auto base_directory = Utils::FilePath {};
            for (const auto &source_group : sources) {
                for (const auto &source : source_group.sources) {
                    const auto source_path = [&] {
                        auto path = Utils::FilePath::fromString(source);

                        if (!path.isAbsolutePath())
                            path = project_dir.resolvePath(Utils::FilePath::fromString(source));

                        return path;
                    }();

                    if (base_directory.isEmpty()) base_directory = source_path.parentDir();
                    else
                        base_directory = Utils::FileUtils::commonPath(base_directory, source_path);
                }
            }

            auto node = createSourceGroupNode(base_directory, "Source Files");
            if (node) {
                buildTargetSourceTree(node.get(), sources);
                parent->addNode(std::move(node));
            }

            if (!target.modules.empty()) {
                base_directory = Utils::FilePath {};
                for (const auto &source : target.modules) {
                    const auto source_path = Utils::FilePath::fromString(source);

                    if (base_directory.isEmpty()) base_directory = source_path.parentDir();
                    else
                        base_directory = Utils::FileUtils::commonPath(base_directory, source_path);
                }

                node = createSourceGroupNode(base_directory, "Module Files");
                if (node) {
                    buildTargetHeaderTree(node.get(), target.modules);
                    parent->addNode(std::move(node));
                }
            }

            if (!target.headers.empty()) {
                base_directory = Utils::FilePath {};
                for (const auto &source : target.headers) {
                    const auto source_path = Utils::FilePath::fromString(source);

                    if (base_directory.isEmpty()) base_directory = source_path.parentDir();
                    else
                        base_directory = Utils::FileUtils::commonPath(base_directory, source_path);
                }

                node = createSourceGroupNode(base_directory, "Header Files");
                if (node) {
                    buildTargetHeaderTree(node.get(), target.headers);
                    parent->addNode(std::move(node));
                }
            }

            if (!(target.packages.empty() && target.frameworks.empty())) {
                auto node =
                    buildTargetExternalPackagesTree(Utils::FilePath::fromString(target.defined_in),
                                                    target.name,
                                                    target.packages,
                                                    target.frameworks);
                if (node) {
                    node->setParentFolderNode(parent);
                    parent->addNode(std::move(node));
                }
            }

            target_paths.insert(Utils::FilePath::fromString(target.defined_in).absolutePath());
        }

        for (auto bs_file : bs_files) {
            if (!bs_file.toFileInfo().isAbsolute())
                bs_file = src_dir.pathAppended(bs_file.toString());

            project_node->findNode([&project_node, &bs_file](ProjectExplorer::Node *node) {
                qCDebug(xmake_project_tree_log) << node->filePath();
                if (node->filePath() == bs_file.absolutePath()) {
                    auto *as_folder = dynamic_cast<ProjectExplorer::FolderNode *>(node);
                    if (as_folder != nullptr) {
                        auto project_file_node = std::make_unique<ProjectExplorer::FileNode>(
                            bs_file.absoluteFilePath(),
                            ProjectExplorer::FileType::Project);
                        qCDebug(xmake_project_tree_log)
                            << "Project file node " << bs_file.toUserOutput();

                        as_folder->addNode(std::move(project_file_node));
                    }

                    return true;
                }

                return false;
            });
        }

        return project_node;
    }
} // namespace XMakeProjectManager::Internal
