#include "DependencyResolver.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace ChonkyStation4::Core::Loader {

namespace {

enum class VisitState {
    Unvisited,
    Visiting,
    Visited,
};

void Visit(
    const ModuleRegistry& registry,
    const std::string& name,
    std::unordered_map<std::string, VisitState>& states,
    std::vector<std::string>& path,
    DependencyResolutionReport& report) {
    const auto state = states[name];
    if (state == VisitState::Visited) {
        return;
    }
    if (state == VisitState::Visiting) {
        const auto cycleStart = std::find(path.begin(), path.end(), name);
        if (cycleStart != path.end()) {
            report.circularDependencies.assign(cycleStart, path.end());
            report.circularDependencies.push_back(name);
        } else {
            report.circularDependencies = {name};
        }
        report.success = false;
        report.error = "circular module dependency detected";
        return;
    }

    const auto* module = registry.Find(name);
    if (module == nullptr) {
        report.missingDependencies.push_back(name);
        report.success = false;
        report.error = "missing module dependency: " + name;
        return;
    }

    states[name] = VisitState::Visiting;
    path.push_back(name);
    for (const auto& dependency : module->Dependencies()) {
        if (registry.Find(dependency.name) == nullptr) {
            if (dependency.required) {
                report.missingDependencies.push_back(dependency.name);
                report.success = false;
                report.error = "missing module dependency: " + dependency.name;
            }
            continue;
        }
        Visit(registry, dependency.name, states, path, report);
    }
    path.pop_back();
    states[name] = VisitState::Visited;
    if (std::find(report.loadOrder.begin(), report.loadOrder.end(), name) ==
        report.loadOrder.end()) {
        report.loadOrder.push_back(name);
    }
}

} // namespace

DependencyResolver::DependencyResolver(const ModuleRegistry& registry) noexcept
    : registry_(&registry) {}

DependencyResolutionReport DependencyResolver::Resolve(const std::string& moduleName) const {
    DependencyResolutionReport report;
    if (registry_ == nullptr || registry_->Find(moduleName) == nullptr) {
        report.error = "module is not registered: " + moduleName;
        report.missingDependencies.push_back(moduleName);
        return report;
    }

    std::unordered_map<std::string, VisitState> states;
    std::vector<std::string> path;
    Visit(*registry_, moduleName, states, path, report);
    report.success = report.error.empty() && report.circularDependencies.empty();

    std::ostringstream log;
    const auto* root = registry_->Find(moduleName);
    log << "Loading module:\n\n" << (root != nullptr ? root->Name() : moduleName)
        << "\n\n";
    if (root != nullptr) {
        for (const auto& dependency : root->Dependencies()) {
            log << "Dependency:\n" << dependency.name
                << "\n\nSearching registry...\n\n"
                << (registry_->Find(dependency.name) != nullptr
                    ? "Found:\nSUCCESS\n\n"
                    : "Missing:\nFAILURE\n\n");
        }
    }
    if (!report.circularDependencies.empty()) {
        log << "Circular dependency:\n";
        for (std::size_t index = 0; index < report.circularDependencies.size(); ++index) {
            if (index != 0) {
                log << " -> ";
            }
            log << report.circularDependencies[index];
        }
        log << "\n";
    }
    if (!report.error.empty()) {
        log << "Error:\n" << report.error << "\n";
    }
    report.log = log.str();
    return report;
}

} // namespace ChonkyStation4::Core::Loader
