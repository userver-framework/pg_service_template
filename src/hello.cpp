#include "hello.hpp"

#include <fmt/format.h>

#include <userver/clients/dns/component.hpp>
#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/assert.hpp>

namespace service_template {

namespace {

class Hello final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-hello";

  Hello(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("postgres-db-1")
                .GetCluster()) {}

  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext&) const override {
    const auto& name = request.GetArg("name");

    auto user_type = UserType::kFirstTime;
    if (!name.empty()) {
      auto result = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "INSERT INTO hello_schema.users(name, count) VALUES($1, 1) "
          "ON CONFLICT (name) "
          "DO UPDATE SET count = users.count + 1 "
          "RETURNING users.count",
          name);

      if (result.AsSingleRow<int>() > 1) {
        user_type = UserType::kKnown;
      }
    }

    return service_template::SayHelloTo(name, user_type);
  }

  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

std::string SayHelloTo(std::string_view name, UserType type) {
  if (name.empty()) {
    name = "unknown user";
  }

  switch (type) {
    case UserType::kFirstTime:
      return fmt::format("Hello, {}!\n", name);
    case UserType::kKnown:
      return fmt::format("Hi again, {}!\n", name);
  }

  UASSERT(false);
}

void AppendHello(userver::components::ComponentList& component_list) {
  component_list.Append<Hello>();
  component_list.Append<userver::components::Postgres>("postgres-db-1");
  component_list.Append<userver::clients::dns::Component>();
}

}  // namespace service_template
