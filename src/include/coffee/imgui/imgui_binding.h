#pragma once

#include <coffee/comp_app/services.h>
#include <coffee/comp_app/subsystems.h>
#include <coffee/core/libc_types.h>
#include <coffee/core/stl_types.h>
#include <peripherals/stl/string_ops.h>
#include <platforms/process.h>
#include <platforms/sysinfo.h>

#include <coffee/graphics/apis/gleam/rhi.h>

#include <imgui.h>

namespace imgui {
namespace detail {

using namespace Coffee::Components;
using type_safety::empty_list_t;
using type_safety::type_list_t;

struct ImGuiWidget
{
    using value_type = ImGuiWidget;
    using type       = Allocators::VectorContainer<ImGuiWidget>;

    std::string_view name;
    std::function<void(EntityContainer&, time_point const&, duration const&)>
        func;
};

struct ImGuiData;
struct ImGuiDataDeleter
{
    void operator()(ImGuiData* p);
};

struct ImGuiSystem : RestrictedSubsystem<
                         ImGuiSystem,
                         SubsystemManifest<
                             type_safety::type_list_t<ImGuiWidget>,
                             type_safety::empty_list_t,
                             type_safety::type_list_t<
                                 comp_app::DisplayInfo,
                                 comp_app::KeyboardInput,
                                 comp_app::MouseInput,
                                 comp_app::TouchInput,
                                 comp_app::Windowing>>>,
                     comp_app::AppLoadableService
{
    ImGuiSystem(gleam::api& api);

    using type = ImGuiSystem;

    virtual const ImGuiSystem& get() const final
    {
        return *this;
    }
    virtual ImGuiSystem& get() final
    {
        return *this;
    }

    virtual void load(entity_container& e, comp_app::app_error& ec) final;
    virtual void unload(entity_container& e, comp_app::app_error& ec) final;
    virtual void start_restricted(Proxy& p, time_point const& t) final;
    virtual void end_restricted(Proxy&, time_point const&) final;

    ImGuiSystem& addWidget(ImGuiWidget&& widget);

  private:
    std::unique_ptr<ImGuiData, ImGuiDataDeleter> m_im_data;

    gleam::api& m_api;
    time_point  m_previousTime;
    bool        m_textInputActive;
};

} // namespace detail

namespace widgets {

extern detail::ImGuiWidget StatsMenu();

} // namespace widgets

using detail::ImGuiSystem;
using detail::ImGuiWidget;

} // namespace imgui
