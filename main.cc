#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <iostream>
#include <linux/input.h>
#include <linux/uinput.h>

namespace asio = boost::asio;

namespace {

constexpr const auto AbsMax{255};
constexpr const auto AbsMin{0};
constexpr const auto AbsMid{127};
constexpr const auto AbsInputMax{255};
constexpr const auto AbsInputMin{0};
constexpr const auto AbsOffset{20};

std::map<int, int> keymap{
    {KEY_L, BTN_A},
    {KEY_SEMICOLON, BTN_B},
    {KEY_K, BTN_X},
    {KEY_O, BTN_Y},
    {KEY_Q, BTN_TL2},
    {KEY_E, BTN_TL},
    {KEY_I, BTN_TR},
    {KEY_P, BTN_TR2},
    {KEY_BACKSPACE, BTN_SELECT},
    {KEY_ENTER, BTN_START},
    {KEY_2, BTN_DPAD_LEFT},
    {KEY_3, BTN_DPAD_RIGHT},
    {KEY_0, BTN_DPAD_UP},
    {KEY_MINUS, BTN_DPAD_DOWN},
};

std::map<int, std::pair<int, bool>> absmap{
    {KEY_A, {ABS_X, false}}, {KEY_D, {ABS_X, true}},   {KEY_W, {ABS_Y, false}},
    {KEY_S, {ABS_Y, true}},  {KEY_F, {ABS_RX, false}}, {KEY_J, {ABS_RX, true}}};

} // namespace

struct input_device {
  input_device() {
    _open();

    _ioctl(UI_SET_EVBIT, EV_KEY);

    for (auto k : {BTN_A, BTN_B, BTN_X, BTN_Y, BTN_TL, BTN_TL2, BTN_TR, BTN_TR2,
                   BTN_START, BTN_SELECT, BTN_DPAD_UP, BTN_DPAD_DOWN,
                   BTN_DPAD_LEFT, BTN_DPAD_RIGHT, BTN_THUMBL, BTN_THUMBR}) {
      _ioctl(UI_SET_KEYBIT, k);
    }

    _ioctl(UI_SET_EVBIT, EV_ABS);

    for (auto k : {ABS_X, ABS_Y, ABS_RX, ABS_RY}) {
      _ioctl(UI_SET_ABSBIT, k);
    }

    uinput_setup usetup;
    bzero(&usetup, sizeof(uinput_setup));

    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x3;
    usetup.id.product = 0x3;
    usetup.id.version = 2;
    strcpy(usetup.name, "Test Dev");

    _ioctl(UI_DEV_SETUP, &usetup);

    {
      uinput_abs_setup uabssetup;
      bzero(&uabssetup, sizeof(uinput_abs_setup));
      uabssetup.absinfo.value = AbsMid;
      uabssetup.absinfo.maximum = AbsMax;
      uabssetup.absinfo.minimum = AbsMin;
      uabssetup.absinfo.fuzz = 0;
      uabssetup.absinfo.flat = 0;
      uabssetup.code = ABS_X;
      _ioctl(UI_ABS_SETUP, &uabssetup);
    }

    {
      uinput_abs_setup uabssetup;
      bzero(&uabssetup, sizeof(uinput_abs_setup));
      uabssetup.absinfo.value = AbsMid;
      uabssetup.absinfo.maximum = AbsMax;
      uabssetup.absinfo.minimum = AbsMin;
      uabssetup.absinfo.fuzz = 0;
      uabssetup.absinfo.flat = 0;
      uabssetup.code = ABS_Y;
      _ioctl(UI_ABS_SETUP, &uabssetup);
    }

    {
      uinput_abs_setup uabssetup;
      bzero(&uabssetup, sizeof(uinput_abs_setup));
      uabssetup.absinfo.value = AbsMid;
      uabssetup.absinfo.maximum = AbsMax;
      uabssetup.absinfo.minimum = AbsMin;
      uabssetup.absinfo.fuzz = 0;
      uabssetup.absinfo.flat = 0;
      uabssetup.code = ABS_RX;
      _ioctl(UI_ABS_SETUP, &uabssetup);
    }

    {
      uinput_abs_setup uabssetup;
      bzero(&uabssetup, sizeof(uinput_abs_setup));
      uabssetup.absinfo.value = AbsMid;
      uabssetup.absinfo.maximum = AbsMax;
      uabssetup.absinfo.minimum = AbsMin;
      uabssetup.absinfo.fuzz = 0;
      uabssetup.absinfo.flat = 0;
      uabssetup.code = ABS_RY;
      _ioctl(UI_ABS_SETUP, &uabssetup);
    }

    _ioctl(UI_DEV_CREATE);
  }

  ~input_device() {
    _ioctl(UI_DEV_DESTROY);
    close(this->fd);
  }

  void emit(input_event const &ie) {
    write(fd, &ie, sizeof(input_event));

    input_event sync;
    sync.code = 0;
    sync.type = EV_SYN;
    sync.value = 0;
    sync.time.tv_sec = 0;
    sync.time.tv_usec = 0;
    write(fd, &sync, sizeof(input_event));
  }

private:
  void _open() {
    this->fd = open("/dev/uinput", O_WRONLY);
    if (this->fd == -1) {
      throw std::runtime_error(
          (boost::format("create dev error: %s") % std::strerror(errno)).str());
    }
  }

  template <typename... Args> void _ioctl(Args &&...args) {
    if (ioctl(this->fd, std::forward<Args>(args)...) == -1) {
      throw std::runtime_error(
          (boost::format("ioctl error: %s") % std::strerror(errno)).str());
    }
  }

  int fd;
};

struct input_event_receiver : std::enable_shared_from_this<input_event_receiver> {

  struct abs_event {
    bool press;
    bool increase;
    input_event ev;
  };
  using abs_map = std::map<int, abs_event>;
  using input_event_vec = std::vector<input_event>;

  input_event_receiver(asio::io_context &ctx, std::string const &file)
      : dev{std::make_shared<input_device>()},
        sd{std::make_shared<asio::posix::stream_descriptor>(
            ctx, open(file.c_str(), O_RDONLY))},
        evs{std::make_shared<input_event_vec>()},
        map{std::make_shared<abs_map>()} {
    evs->resize(32);
  }

  void emit(input_event const &ev) {
    {
      auto it = keymap.find(ev.code);
      if (it != keymap.end()) {
        input_event e{ev};
        e.type = EV_KEY;
        e.code = keymap[ev.code];
        dev->emit(e);
      }
    }
    {
      auto it = absmap.find(ev.code);
      if (it != absmap.end()) {
        abs_event abs_ev;
        abs_ev.press = ev.value;
        abs_ev.increase = absmap[ev.code].second;

        abs_ev.ev.type = EV_ABS;
        abs_ev.ev.code = absmap[ev.code].first;
        abs_ev.ev.value = AbsMid + (abs_ev.increase ? AbsOffset : -AbsOffset);
        abs_ev.ev.time.tv_sec = 0;
        abs_ev.ev.time.tv_usec = 0;

        this->update_abs(ev.code, abs_ev);
      }
    }
  }

  void update_abs(int code, abs_event const &e) {
    map->emplace(code, e);

    auto &abs = map->at(code);
    if (!e.press) {
      abs.ev.value = AbsMid;
      abs.press = false;
    } else {
      abs.press = true;
    }

    dev->emit(abs.ev);
  }

  void operator()(boost::system::error_code ec, std::size_t size) {
    if (ec) {
      std::cout << ec.message() << std::endl;
      return;
    }

    auto n = size / sizeof(input_event);
    for (auto i{0}; i < n; ++i) {
      emit(evs->at(i));
    }

    sd->async_read_some(asio::buffer(*evs), *this);
  }

  std::shared_ptr<input_device> dev;
  std::shared_ptr<asio::posix::stream_descriptor> sd;
  std::shared_ptr<input_event_vec> evs;
  std::shared_ptr<abs_map> map;
};

int main(int argc, char *argv[]) {

  asio::io_context ctx;

  auto ev{std::make_shared<input_event_receiver>(ctx, "/dev/input/event3")};
  (*ev)({}, 0);

  asio::steady_timer timer{ctx};

  std::function<void(boost::system::error_code)> cb;

  cb = [&, ev](auto ec) {
    if (ec) {
      std::cout << ec.message() << std::endl;
      return;
    }

    for (auto &[code, abs] : *ev->map) {
      if (abs.press) {
        if (abs.increase) {
          abs.ev.value += AbsOffset;
          abs.ev.value = std::min(AbsMax, abs.ev.value);
        } else {
          abs.ev.value -= AbsOffset;
          abs.ev.value = std::max(AbsMin, abs.ev.value);
        }
        ev->dev->emit(abs.ev);
      }
    }
    timer.expires_from_now(std::chrono::milliseconds(20));
    timer.async_wait(cb);
  };

  cb({});

  ctx.run();
  return 0;
}
