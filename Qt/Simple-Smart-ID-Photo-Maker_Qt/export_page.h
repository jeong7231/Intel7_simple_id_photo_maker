#ifndef EXPORT_PAGE_H
#define EXPORT_PAGE_H

#include <QResizeEvent>
#include <QWidget>
#include <opencv2/opencv.hpp>

namespace Ui
{
class export_page;
}

class export_page : public QWidget
{
    Q_OBJECT

  public:
    explicit export_page(QWidget *parent = nullptr);
    ~export_page();
    void setResultImage(const cv::Mat &image);

  protected:
    void resizeEvent(QResizeEvent *event) override;

  private slots:
    void on_file_format_select_combo_currentTextChanged(const QString &text);
    void on_export_button_clicked();

  private:
    Ui::export_page *ui;
    cv::Mat resultImage;
    QString selectedFormat;
    QString generateUniqueFileName(const QString &baseName, const QString &extension);
};

#endif // EXPORT_PAGE_H
