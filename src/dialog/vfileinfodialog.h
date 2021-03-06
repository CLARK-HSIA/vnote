#ifndef VFILEINFODIALOG_H
#define VFILEINFODIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;
class QDialogButtonBox;
class QString;

class VFileInfoDialog : public QDialog
{
    Q_OBJECT
public:
    VFileInfoDialog(const QString &title, const QString &info, const QString &defaultName,
                    QWidget *parent = 0);
    QString getNameInput() const;

private slots:
    void enableOkButton();

private:
    void setupUI();

    QLabel *infoLabel;
    QLabel *nameLabel;
    QLineEdit *nameEdit;
    QDialogButtonBox *m_btnBox;

    QString title;
    QString info;
    QString defaultName;
};

#endif // VFILEINFODIALOG_H
