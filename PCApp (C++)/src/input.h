#pragma once
#include <QPointF>
#include <QString>
#include <set>

class InputBackend {
  public:
    virtual ~InputBackend() = default;
    virtual void move(int dx, int dy) = 0;
    virtual void point(int monitor, double x, double y) = 0;
    virtual void button(int button, bool down) = 0;
    virtual void scroll(int amount) = 0;
    virtual void key(int javaKey, bool down) = 0;
    virtual void text(const QString &value) = 0;
    virtual void media(const QString &action) = 0;
    virtual void releaseAll() noexcept = 0;
};
class NativeInput final : public InputBackend {
  public:
    NativeInput();
    ~NativeInput() override;
    void move(int dx, int dy) override;
    void point(int monitor, double x, double y) override;
    void button(int button, bool down) override;
    void scroll(int amount) override;
    void key(int javaKey, bool down) override;
    void text(const QString &value) override;
    void media(const QString &action) override;
    void releaseAll() noexcept override;

  private:
    void *display = nullptr;
    std::set<int> keys;
    std::set<int> buttons;
};
