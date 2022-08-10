#ifndef CONTROL_HH
#define CONTROL_HH

#include <flecsi/flog.hh>
#include <flecsi/run/control.hh>

enum class cp { advance };

inline const char *
operator*(cp control_point) {
  switch(control_point) {
    case cp::advance:
      return "advance";
  }
  flog_fatal("invalid control point");
}

struct control_policy {

  using control_points_enum = cp;
  struct node_policy {};

  using control = flecsi::run::control<control_policy>;

  template<auto CP>
  using control_point = flecsi::run::control_point<CP>;

  using control_points = std::tuple<control_point<cp::advance>>;
};

using control = flecsi::run::control<control_policy>;
#endif
